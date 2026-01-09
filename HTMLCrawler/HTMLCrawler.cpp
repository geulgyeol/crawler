#include "../Library/Library.cpp"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;
namespace pubsub = ::google::cloud::pubsub;

const int CRAWL_PER_SECOND_N = CRAWL_PER_SECOND_MAP.at("HTMLCrawler_N");
const int CRAWL_PER_SECOND_T = CRAWL_PER_SECOND_MAP.at("HTMLCrawler_T");
const int DELAY_MILLI_N = 1000 / CRAWL_PER_SECOND_N;
const int DELAY_MILLI_T = 1000 / CRAWL_PER_SECOND_T;
std::chrono::nanoseconds DELAY_NANOS_N = std::chrono::nanoseconds(1'000'000'000LL / CRAWL_PER_SECOND_N);
std::chrono::nanoseconds DELAY_NANOS_T = std::chrono::nanoseconds(1'000'000'000LL / CRAWL_PER_SECOND_T);

unique_ptr<pubsub::Publisher> blogProfilePublisher;
unique_ptr<pubsub::Publisher> blogWritingPublisher;

unique_ptr<pubsub::Subscriber> blogProfileSubscriber;
unique_ptr<pubsub::Subscriber> blogWritingLinkForProfileSubscriber;
unique_ptr<pubsub::Subscriber> blogWritingLinkForContentSubscriber;


queue<string> messageQueue;
bool subscribeEnabled = false;


map<CURL*, struct curl_slist*> headersMap;

CURL* CreateHandle(CURLM* multi_handle, const string link, map<CURL*, string*>& buffers, map<CURL*, string>& link_data, CURL* curl) {
    if (link.empty()) return nullptr;

    size_t pos = link.find('/');
    string link_t = link;
    if (pos == string::npos) {
        return nullptr;
    }
    if (!CheckLinkNotVisited(curl, "Crawler_" + link_t.replace(pos, 1, "%20"))) return nullptr;

    auto start = std::chrono::steady_clock::now();

    CURL* eh = curl_easy_init();
    if (!eh) {
        cerr << "Failed to initialize CURL easy handle." << endl;
        return nullptr;
    }

    int slashIndex = link.find('/');
    string profileName = link.substr(1, slashIndex - 1);
    string writingNumber = link.substr(slashIndex + 1);
    string url;
    string* readBuffer = new string();

    if (link[0] == 'N') {
        url = "https://blog.naver.com/PostView.nhn?blogId=" + profileName + "&logNo=" + writingNumber;
    }
    else if (link[0] == 'T') {
        url = "https://" + profileName + ".tistory.com/m/" + writingNumber;
    }
    else {
        delete readBuffer;
        curl_easy_cleanup(eh);
        return nullptr;
    }

    if (!IsAllowedByRobotsGeneral(url)) {
        cout << "SKIP: Robots.txt denied access for [" << link << "] URL [" << url << "]\\n";
        delete readBuffer;
        curl_easy_cleanup(eh);
        return nullptr;
    }

    struct curl_slist* headers = SetCURL(eh, readBuffer, url);

    if (headers) {
        headersMap.insert({ eh, headers });
    }

    curl_multi_add_handle(multi_handle, eh);
    buffers[eh] = readBuffer;
    link_data[eh] = link;

    string log = "Handle Created in " + GetTakenTime(start) + "\n";
    cout << log;

    return eh;
}


int main() {
    cin.tie(NULL);
    ios::sync_with_stdio(false);

    blogProfilePublisher = make_unique<pubsub::Publisher>(pubsub::Publisher(pubsub::MakePublisherConnection(pubsub::Topic(PROJECT_ID, PROFILE_TOPIC_ID), google::cloud::Options{}.set<pubsub::MessageOrderingOption>(true))));
    blogWritingPublisher = make_unique<pubsub::Publisher>(pubsub::Publisher(pubsub::MakePublisherConnection(pubsub::Topic(PROJECT_ID, WRITING_TOPIC_ID), google::cloud::Options{}.set<pubsub::MessageOrderingOption>(true))));

    blogProfileSubscriber = make_unique<pubsub::Subscriber>(pubsub::Subscriber(pubsub::MakeSubscriberConnection(pubsub::Subscription(PROJECT_ID, PROFILE_SUB_ID))));
    blogWritingLinkForProfileSubscriber = make_unique<pubsub::Subscriber>(pubsub::Subscriber(pubsub::MakeSubscriberConnection(pubsub::Subscription(PROJECT_ID, WRITING_FOR_PROFILE_SUB_ID))));
    blogWritingLinkForContentSubscriber = make_unique<pubsub::Subscriber>(pubsub::Subscriber(pubsub::MakeSubscriberConnection(pubsub::Subscription(PROJECT_ID, WRITING_FOR_CONTENT_SUB_ID))));

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    curl_global_init(CURL_GLOBAL_DEFAULT);

    CURL* curl;
    curl = curl_easy_init();
    CURLM* multi_handle = curl_multi_init();

    if (!multi_handle) {
        cerr << "Failed to initialize CURL multi handle." << endl;
        if (curl) curl_easy_cleanup(curl);
        curl_global_cleanup();
        return 0;
    }
    if (!curl) {
        cerr << "Failed to initialize CURL easy handle for sync tasks." << endl;
        curl_multi_cleanup(multi_handle);
        curl_global_cleanup();
        return 0;
    }

    map<CURL*, string*> buffers;
    map<CURL*, string> link_data;

    thread htmlCrawlerSubscribeThread(Subscribe, move(*blogWritingLinkForContentSubscriber), &messageQueue, &subscribeEnabled, DEFAULT_SUB_WAITING_TIME);
    htmlCrawlerSubscribeThread.detach();

    int links_index = 0;
    int cnt = 0;

    map<string, string> bodies;

    auto start = std::chrono::steady_clock::now();

    using clock = std::chrono::steady_clock;
    auto nextAdd = clock::now();
    int running_handles = 0;

    while (true) {
        {
            auto now = clock::now();

            while ((int)buffers.size() < MAX_CONCURRENT_REQUESTS && now >= nextAdd) {
                std::string link_to_process;
                bool has;
                {
                    std::lock_guard<std::mutex> lock(messageQueueMutex);
                    has = !messageQueue.empty();
                    if (has) {
                        link_to_process = std::move(messageQueue.front());
                        messageQueue.pop();
                    }
                }
                if (!has) {
                    cout << "Queue is empty" << endl;
                    break;
                }

                auto current_delay = (link_to_process[0] == 'N') ? DELAY_NANOS_N : DELAY_NANOS_T;

                CURL* eh = CreateHandle(multi_handle, link_to_process, buffers, link_data, curl);
                if (eh) {
                    nextAdd += current_delay;
                }

                now = clock::now();
            }
        }

        if (!buffers.empty()) {
            int numfds = 0;
            CURLMcode mc = curl_multi_wait(multi_handle, NULL, 0, 10, &numfds);
            if (mc != CURLM_OK) break;

            mc = curl_multi_perform(multi_handle, &running_handles);
            if (mc != CURLM_OK) break;
        } else {
            Delay(100, "main");
        }

        CURLMsg* msg;
        int msgs_left;
        while ((msg = curl_multi_info_read(multi_handle, &msgs_left))) {
            if (msg->msg == CURLMSG_DONE) {
                auto start_ = std::chrono::steady_clock::now();

                CURL* eh = msg->easy_handle;

                string* buffer = buffers[eh];
                string link = link_data[eh];
                long response_code;
                curl_easy_getinfo(eh, CURLINFO_RESPONSE_CODE, &response_code);
                double total_time = 0.0;
                curl_easy_getinfo(eh, CURLINFO_TOTAL_TIME, &total_time);

                if (ENABLE_DB_UPLOAD) {
                    string Body;
                    Body.append("{\"body\":\"" + EscapeQuotes(*buffer) + "\",\"blog\":\"");
                    if (!link.empty() && link[0] == 'N') {
                        Body.append("naver");
                    }
                    else if (!link.empty() && link[0] == 'T') {
                        Body.append("tistory");
                    }
                    Body.append("\",\"timestamp\":" + to_string(chrono::duration_cast<chrono::seconds>(chrono::system_clock::now().time_since_epoch()).count()) + "}");

                    size_t pos = link.find('/');
                    if (pos != string::npos) {
                        string db_link = link;
                        db_link.replace(pos, 1, "%20");
                        string html_storage_link = link;
                        html_storage_link.replace(pos, 1, " ");
                        if (RegisterLink(curl, "Crawler_" + db_link)) {
                            bodies.insert({ html_storage_link.substr(1), Body });
                        }
                    }
                }

                auto it = headersMap.find(eh);
                if (it != headersMap.end()) {
                    curl_slist_free_all(it->second);
                    headersMap.erase(it);
                }

                if (msg->data.result == CURLE_OK && response_code < 400) {
                    string log = "success: " + to_string(++cnt) + " " + GetTakenTime(start_) + "\n";
                    cout << log;
                }
                else {
                    string log = "FAILED for [" + link + "] (Code: " + to_string(response_code) + "). Error: " + curl_easy_strerror(msg->data.result) + " " + GetTakenTime(start_) + " Taken: " + to_string(total_time) + "s\n";
                    cerr << log;

                    // re-publish the failed link
                    Publish(*blogWritingPublisher, { link });
                }

                delete buffer;
                buffers.erase(eh);
                link_data.erase(eh);
                curl_multi_remove_handle(multi_handle, eh);
                curl_easy_cleanup(eh);
            }
        }

        if (bodies.empty() || bodies.size() < BODIES_THRESHOLD) continue;

        string log = to_string(bodies.size()) + " HTML Crawled in " + GetTakenTime(start) + "\n";
        cout << log;
        start = std::chrono::steady_clock::now();
        thread postHTMLContentThread(PostHTMLContent, std::move(bodies));
        postHTMLContentThread.detach();
        bodies.clear();
    }

    if (curl) {
        curl_easy_cleanup(curl);
    }
    curl_multi_cleanup(multi_handle);
    curl_global_cleanup();

    return 0;
}

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

unique_ptr<pubsub::Publisher> blogProfilePublisher;
unique_ptr<pubsub::Publisher> blogWritingPublisher;

unique_ptr<pubsub::Subscriber> blogProfileSubscriber;
unique_ptr<pubsub::Subscriber> blogWritingLinkForProfileSubscriber;
unique_ptr<pubsub::Subscriber> blogWritingLinkForContentSubscriber;

vector<struct curl_slist*> headersList;

CURL* CreateHandle(CURLM* multi_handle, const string link, map<CURL*, string*>& buffers, map<CURL*, string>& link_data, CURL* curl) {
    if (link.empty()) return nullptr;

    size_t pos = link.find('/');
    string link_t = link;
    if (pos == string::npos) {
        return nullptr;
    }
    if (!CheckLinkNotVisited(curl, "Crawler_" + link_t.replace(pos, 1, "%20"))) return nullptr;

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
        Delay(link[0], DELAY_MILLI_N, DELAY_MILLI_T);
        return nullptr;
    }

    headersList.push_back(SetCURL(eh, readBuffer, url));

    curl_multi_add_handle(multi_handle, eh);
    buffers[eh] = readBuffer;
    link_data[eh] = link;

    return eh;
}


int main() {
    cin.tie(NULL);
    ios::sync_with_stdio(false);

    blogProfilePublisher = make_unique<pubsub::Publisher>(pubsub::Publisher(pubsub::MakePublisherConnection(pubsub::Topic(PROJECT_ID, PROFILE_TOPIC_ID))));
    blogWritingPublisher = make_unique<pubsub::Publisher>(pubsub::Publisher(pubsub::MakePublisherConnection(pubsub::Topic(PROJECT_ID, WRITING_TOPIC_ID))));

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

    int links_index = 0;
    int cnt = 0;

    while (true) {
        vector<string> links = Subscribe(*blogWritingLinkForContentSubscriber, 100);

        if (links[0] == TIMEOUTED) {
            if (buffers.empty()) continue;
            links.clear();
            links_index = 0;
        }
        else {
            links_index = 0;
        }

        int running_handles = 0;

        while (links_index < links.size() || running_handles > 0) {

            while (running_handles < MAX_CONCURRENT_REQUESTS && links_index < links.size()) {
                CURL* eh = CreateHandle(multi_handle, links[links_index], buffers, link_data, curl);
                if (eh) {
                    curl_multi_perform(multi_handle, &running_handles);
                }
                links_index++;
            }

            if (running_handles > 0) {
                int numfds = 0;
                CURLMcode mc = curl_multi_wait(multi_handle, NULL, 0, 1000, &numfds);
                if (mc != CURLM_OK) break;

                mc = curl_multi_perform(multi_handle, &running_handles);
                if (mc != CURLM_OK) break;
            }

            CURLMsg* msg;
            int msgs_left;
            while ((msg = curl_multi_info_read(multi_handle, &msgs_left))) {
                if (msg->msg == CURLMSG_DONE) {
                    CURL* eh = msg->easy_handle;

                    string* buffer = buffers[eh];
                    string link = link_data[eh];
                    long response_code;
                    curl_easy_getinfo(eh, CURLINFO_RESPONSE_CODE, &response_code);

                    if (msg->data.result == CURLE_OK && response_code < 400) {
                        cout << "success: " << ++cnt << "\n";
                    }
                    else {
                        cerr << "FAILED for [" << link << "] (Code: " << response_code << "). Error: " << curl_easy_strerror(msg->data.result) << endl;
                    }

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
                            link.replace(pos, 1, "%20");
                            if (RegisterLink(curl, "Crawler_" + link)) {
                                PostHTMLContent(curl, link.substr(1), Body);
                            }
                        }
                    }

                    delete buffer;
                    buffers.erase(eh);
                    link_data.erase(eh);
                    curl_multi_remove_handle(multi_handle, eh);
                    curl_easy_cleanup(eh);

                    Delay(link[0], DELAY_MILLI_N, DELAY_MILLI_T);
                }
            }

            if (links_index >= links.size() && running_handles == 0) {
                break;
            }
        }

        if (!headersList.empty()) {
            for (int j = 0; j < headersList.size(); j++) {
                curl_slist_free_all(headersList[j]);
            }
            headersList.clear();
        }

        cout << "All links processed for this batch. Total remaining handles: " << buffers.size() << endl;
    }

    if (curl) {
        curl_easy_cleanup(curl);
    }
    curl_multi_cleanup(multi_handle);
    curl_global_cleanup();

    return 0;
}

#include "../Library/Library.cpp"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;
namespace pubsub = ::google::cloud::pubsub;

const int CRAWL_PER_SECOND_N = CRAWL_PER_SECOND_MAP.at("LinkFinder_N");
const int CRAWL_PER_SECOND_T = CRAWL_PER_SECOND_MAP.at("LinkFinder_T");
const int DELAY_MILLI_N = 1000 / CRAWL_PER_SECOND_N;
const int DELAY_MILLI_T = 1000 / CRAWL_PER_SECOND_T;

unique_ptr<pubsub::Publisher> blogProfilePublisher;
unique_ptr<pubsub::Publisher> blogWritingPublisher;

unique_ptr<pubsub::Subscriber> blogProfileSubscriber;
unique_ptr<pubsub::Subscriber> blogWritingLinkForProfileSubscriber;
unique_ptr<pubsub::Subscriber> blogWritingLinkForContentSubscriber;


queue<Message> messageQueue;
queue<int> deleteQueue;
bool subscribeEnabled = false;

struct TistoryRequestData {
    string* buffer;
    int index;
};


int main() {
    cin.tie(NULL);
    ios::sync_with_stdio(false);

    blogProfilePublisher = make_unique<pubsub::Publisher>(pubsub::Publisher(pubsub::MakePublisherConnection(pubsub::Topic(PROJECT_ID, PROFILE_TOPIC_ID))));
    blogWritingPublisher = make_unique<pubsub::Publisher>(pubsub::Publisher(pubsub::MakePublisherConnection(pubsub::Topic(PROJECT_ID, WRITING_TOPIC_ID))));

    blogProfileSubscriber = make_unique<pubsub::Subscriber>(pubsub::Subscriber(pubsub::MakeSubscriberConnection(pubsub::Subscription(PROJECT_ID, PROFILE_SUB_ID))));
    blogWritingLinkForProfileSubscriber = make_unique<pubsub::Subscriber>(pubsub::Subscriber(pubsub::MakeSubscriberConnection(pubsub::Subscription(PROJECT_ID, WRITING_FOR_PROFILE_SUB_ID))));
    blogWritingLinkForContentSubscriber = make_unique<pubsub::Subscriber>(pubsub::Subscriber(pubsub::MakeSubscriberConnection(pubsub::Subscription(PROJECT_ID, WRITING_FOR_CONTENT_SUB_ID))));

    //Publish(*blogProfilePublisher, { "Nhaesung_88" });
    //Publish(*blogProfilePublisher, { "Tnelastory" });
    //Publish(*blogProfilePublisher, { "N1_do_everything" });
    //Publish(*blogProfilePublisher, { "Tmungdenson" });

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL* curl;

    thread linkFinderSubscribeThread(GetQueue, "user", &messageQueue);
    linkFinderSubscribeThread.detach();
    thread linkFinderDeleteThread(DeleteQueue, "user", &deleteQueue);
    linkFinderDeleteThread.detach();

    while (true) {
        bool is_empty;
        {
            lock_guard<mutex> lock(messageQueueMutex);
            is_empty = messageQueue.empty();
        }

        if (is_empty) {
            Delay(100, "main");
            continue;
        }

        Message message;
        {
            std::lock_guard<std::mutex> lock(messageQueueMutex);
            message = { messageQueue.front() };
            messageQueue.pop();
        }

        string link = message.message;
        if (message.isLocked()) {
            continue;
        }

        {
            lock_guard<mutex> lock(deleteQueueMutex);
            deleteQueue.push(message.id);
        }

        vector<string*> buffers;

        string readBuffer;
        
        curl = curl_easy_init();
        if (curl) {
            if (link == "") {
                break;
            }

            string link_t = link;
            size_t pos = link_t.find('/');
            if (pos != string::npos) {
                link_t.replace(pos, 1, "%20");
            }

            if (ENABLE_DB_UPLOAD && !CheckLinkNotVisited(curl, "LinkFinder_" + link_t)) continue;

            if (link[0] == 'N') {
                string blogName = link.substr(1);
                vector<string> validPages;
                int collectCnt = 0;
                int currentPage = 1;

                set<string> foundPostIds;
                bool duplicateFound = false;

                while (true) {
                    auto start = std::chrono::steady_clock::now();

                    string url = "https://blog.naver.com/PostTitleListAsync.naver?blogId=" + blogName + "&currentPage=" + to_string(currentPage) + "&countPerPage=30";
                    string referer = "Referer: https://blog.naver.com/" + blogName;

                    if (!IsAllowedByRobotsGeneral(url)) {
                        string log = "SKIP: Robots.txt denied access for [" + link + "] URL [" + url + "]\\n";
                        cout << log;
                        break;
                    }

                    readBuffer.clear();
                    struct curl_slist* headers = SetCURL(curl, &readBuffer, url, referer);

                    CURLcode res = curl_easy_perform(curl);
                    curl_slist_free_all(headers);
                    if (res != CURLE_OK) {
                        cerr << "curl_easy_perform() failed on page " << currentPage << ": " << curl_easy_strerror(res) << endl;
                        break;
                    }

                    if (readBuffer.find("\"resultCode\":\"E\"") != string::npos) {
                        break;
                    }

                    int lastIndex = readBuffer.find("tagQueryString");
                    int pagesFoundInThisCall = 0;

                    while (true) {
                        int newIndex = readBuffer.find("logNo", lastIndex);
                        if (newIndex == string::npos) {
                            break;
                        }

                        int splitIndex = readBuffer.find("&", newIndex);
                        if (splitIndex == string::npos) {
                            splitIndex = readBuffer.find("\"", newIndex);
                        }

                        string postId = readBuffer.substr(newIndex + 6, splitIndex - newIndex - 6);

                        lastIndex = splitIndex;

                        if (foundPostIds.count(postId)) {
                            duplicateFound = true;
                            break;
                        }

                        foundPostIds.insert(postId);
                        validPages.push_back("N" + blogName + "/" + postId);

                        collectCnt++;
                        pagesFoundInThisCall++;
                    }

                    string log = "Current Collect : " + to_string(collectCnt) + " (Page: " + to_string(currentPage) + ") " + GetTakenTime(start) + "\n";
                    cout << log;

                    if (duplicateFound || pagesFoundInThisCall == 0) {
                        break;
                    }

                    currentPage++;
                    Delay(DELAY_MILLI_N, "main");
                }

                cout << "\n";

                if (!ENABLE_DB_UPLOAD || RegisterLink(curl, "LinkFinder_" + link_t)) {
                    PostQueue("profile", validPages);
                    PostQueue("content", validPages);
                }

                Delay(DELAY_MILLI_N, "main");
            }
            else if (link[0] == 'T') {
                string readBuffer;
                string url = "https://" + link.substr(1) + ".tistory.com/rss";

                if (!IsAllowedByRobotsGeneral(url)) {
                    string log = "SKIP: Robots.txt denied access for [" + link + "] URL [" + url + "]\\n";
                    cout << log;
                    Delay(DELAY_MILLI_T, "main");
                    curl_easy_cleanup(curl);
                    continue;
                }

                auto start = std::chrono::steady_clock::now();

                struct curl_slist* headers = SetCURL(curl, &readBuffer, url);
                CURLcode res = curl_easy_perform(curl);
                curl_slist_free_all(headers);

                if (res != CURLE_OK) {
                    cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << endl;
                    curl_easy_cleanup(curl);
                    continue;
                }

                regex linkRegex("<link>https://[^/]+/([0-9]+)</link>");
                smatch match;

                int maxIndex = 0;
                if (regex_search(readBuffer, match, linkRegex)) {
                    try {
                        maxIndex = stoi(match[1].str());
                    }
                    catch (exception& e) {
                        cerr << "Failed to parse max post ID from RSS: " << e.what() << endl;
                        continue;
                    }
                }

                if (maxIndex == 0) {
                    cout << "No post IDs found for [" << link << "]. " + GetTakenTime(start) + "\n";
                    curl_easy_cleanup(curl);
                    Delay(DELAY_MILLI_T, "main");
                    continue;
                }

                CURLM* multi_handle = curl_multi_init();
                if (!multi_handle) {
                    cerr << "Failed to initialize CURL multi handle" << endl;
                    curl_easy_cleanup(curl);
                    continue;
                }

                map<CURL*, unique_ptr<TistoryRequestData>> requests;

                vector<string> validPages;
                int emptyPageCnt = 0;
                int currentIndex = maxIndex;
                int completed = 0;

                string log = "Requests (total max: " + to_string(maxIndex) + ") " + GetTakenTime(start) + "\n";
                cout << log;

                start = std::chrono::steady_clock::now();

                while (currentIndex > 0 || !requests.empty()) {
                    if (currentIndex > 0 && requests.size() < MAX_CONCURRENT_REQUESTS) {
                        CURL* eh = curl_easy_init();
                        if (!eh) {
                            cerr << "Failed to initialize easy handle for index " << currentIndex << endl;
                            currentIndex--;
                            continue;
                        }

                        auto data = make_unique<TistoryRequestData>();
                        data->index = currentIndex;
                        data->buffer = new string();

                        string request_url = "https://" + link.substr(1) + ".tistory.com/" + to_string(currentIndex);

                        if (IsAllowedByRobotsGeneral(request_url)) {
                            struct curl_slist* headers = SetCURL(eh, data->buffer, request_url, "", "0-256");

                            curl_easy_setopt(eh, CURLOPT_PRIVATE, data.get());

                            curl_multi_add_handle(multi_handle, eh);
                            requests[eh] = std::move(data);

                            Delay(DELAY_MILLI_T, "main");
                        }
                        else {
                            cout << "SKIP: Robots.txt denied access for [" << link << "] URL [" << request_url << "]\\n";
                            delete data->buffer;
                            curl_easy_cleanup(eh);
                        }
                        currentIndex--;
                    }

                    int running_handles = requests.size();
                    if (running_handles > 0) {
                        int numfds = 0;
                        CURLMcode mc = curl_multi_wait(multi_handle, NULL, 0, 1, &numfds);
                        if (mc != CURLM_OK) break;

                        curl_multi_perform(multi_handle, &running_handles);

                        CURLMsg* msg;
                        int msgs_left;
                        while ((msg = curl_multi_info_read(multi_handle, &msgs_left))) {
                            if (msg->msg == CURLMSG_DONE) {
                                CURL* eh = msg->easy_handle;

                                TistoryRequestData* raw_data_ptr = nullptr;
                                curl_easy_getinfo(eh, CURLINFO_PRIVATE, &raw_data_ptr);

                                if (raw_data_ptr && requests.count(eh)) {
                                    int titleTagOpenIndex = raw_data_ptr->buffer->find("<title>");
                                    int titleTagCloseIndex = raw_data_ptr->buffer->find("</title>");

                                    string htmlTitle = raw_data_ptr->buffer->substr(titleTagOpenIndex + 7, titleTagCloseIndex - titleTagOpenIndex - 7);

                                    if (htmlTitle != "TISTORY") {
                                        emptyPageCnt = 0;
                                        validPages.push_back("T" + link.substr(1) + "/" + to_string(raw_data_ptr->index));
                                    }
                                    else {
                                        emptyPageCnt++;
                                        if (emptyPageCnt >= 20) {
                                            currentIndex = 0;
                                        }
                                    }

                                    delete raw_data_ptr->buffer;

                                    requests.erase(eh);

                                    curl_multi_remove_handle(multi_handle, eh);
                                    curl_easy_cleanup(eh);
                                    ++completed;
                                    PrintProgressBar(completed, maxIndex);
                                }
                                else {
                                    curl_multi_remove_handle(multi_handle, eh);
                                    curl_easy_cleanup(eh);
                                    ++completed;
                                    PrintProgressBar(completed, maxIndex);
                                }
                            }
                        }
                    }

                    if (currentIndex <= 0 && requests.empty()) {
                        break;
                    }
                }

                log = "\n# Valid Page Count : " + to_string(validPages.size()) + ", " + GetTakenTime(start) + "\n";
                cout << log;
                curl_multi_cleanup(multi_handle);

                if (!ENABLE_DB_UPLOAD || RegisterLink(curl, "LinkFinder_" + link_t)) {
                    PostQueue("profile", validPages);
                    PostQueue("content", validPages);
                }

                Delay(DELAY_MILLI_T, "main");
            }
            cout << "\n";
        }

        if (curl) {
            curl_easy_cleanup(curl);
        }

        if (!buffers.empty()) {
            for (int i = 0; i < buffers.size(); i++) {
                if (buffers[i] != nullptr) {
                    delete buffers[i];
                }
            }
        }
    }

    curl_global_cleanup();

    return 0;
}
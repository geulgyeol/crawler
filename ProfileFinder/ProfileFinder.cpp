#include "../Library/Library.cpp" //109 line

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;
namespace pubsub = ::google::cloud::pubsub;

const int CRAWL_PER_SECOND_N = CRAWL_PER_SECOND_MAP.at("ProfileFinder_N");
const int CRAWL_PER_SECOND_T = CRAWL_PER_SECOND_MAP.at("ProfileFinder_T");
const int DELAY_MILLI_N = 1000 / CRAWL_PER_SECOND_N;
const int DELAY_MILLI_T = 1000 / CRAWL_PER_SECOND_T;

unique_ptr<pubsub::Publisher> blogProfilePublisher;
unique_ptr<pubsub::Publisher> blogWritingPublisher;

unique_ptr<pubsub::Subscriber> blogProfileSubscriber;
unique_ptr<pubsub::Subscriber> blogWritingLinkForProfileSubscriber;
unique_ptr<pubsub::Subscriber> blogWritingLinkForContentSubscriber;


queue<string> messageQueue;
bool subscribeEnabled = false;


int main() {
    cin.tie(NULL);
    ios::sync_with_stdio(false);

    blogProfilePublisher = make_unique<pubsub::Publisher>(pubsub::Publisher(pubsub::MakePublisherConnection(pubsub::Topic(PROJECT_ID, PROFILE_TOPIC_ID), google::cloud::Options{}.set<pubsub::MessageOrderingOption>(true))));
    blogWritingPublisher = make_unique<pubsub::Publisher>(pubsub::Publisher(pubsub::MakePublisherConnection(pubsub::Topic(PROJECT_ID, WRITING_TOPIC_ID), google::cloud::Options{}.set<pubsub::MessageOrderingOption>(true))));

    blogProfileSubscriber = make_unique<pubsub::Subscriber>(pubsub::Subscriber(pubsub::MakeSubscriberConnection(pubsub::Subscription(PROJECT_ID, PROFILE_SUB_ID))));
    blogWritingLinkForProfileSubscriber = make_unique<pubsub::Subscriber>(pubsub::Subscriber(pubsub::MakeSubscriberConnection(pubsub::Subscription(PROJECT_ID, WRITING_FOR_PROFILE_SUB_ID))));
    blogWritingLinkForContentSubscriber = make_unique<pubsub::Subscriber>(pubsub::Subscriber(pubsub::MakeSubscriberConnection(pubsub::Subscription(PROJECT_ID, WRITING_FOR_CONTENT_SUB_ID))));

    //Publish(*blogWritingPublisher, { "Nhaesung_88/223597388359" });
    //Publish(*blogWritingPublisher, { "Tlsas4565/8838853" });
    //Publish(*blogWritingPublisher, { "Tlsas4565/8838853" });
    //Publish(*blogWritingPublisher, { "Tnelastory/35" });
    //Publish(*blogWritingPublisher, { "Tnelastory/37" });

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL* curl;

    thread profileFinderSubscribeThread(Subscribe, *blogWritingLinkForProfileSubscriber, &messageQueue, &subscribeEnabled, DEFAULT_SUB_WAITING_TIME);
    profileFinderSubscribeThread.detach();

    map<string, bool> visited;
    if (!ENABLE_DB_UPLOAD) {
        visited.insert({ "visited map is not empty", true });
    }

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

        string link;
        {
            std::lock_guard<std::mutex> lock(messageQueueMutex);
            link = { messageQueue.front() };
            messageQueue.pop();
        }

        string readBuffer;

        curl = curl_easy_init();
        if (curl) {
            if (link == "") {
                break;
            }

            int slashIndex = link.find('/');
            string profileName = link.substr(1, slashIndex - 1);
            string writingNumber = link.substr(slashIndex + 1);

            if (link[0] == 'N') {
                string url = "https://blog.naver.com/api/blogs/" + profileName + "/posts/" + writingNumber + "/sympathy-users?itemCount=100&timeStamp=9999999999999";
                string referer = "https://blog.naver.com/SympathyHistoryList.naver?blogId=" + profileName + "&logNo=" + writingNumber;

                if (!IsAllowedByRobotsGeneral(url)) {
                    cout << "SKIP: Robots.txt denied access for [" << link << "] URL [" << url << "]\\n";
                    Delay(DELAY_MILLI_N, "main");
                    continue;
                }

                struct curl_slist* headers = SetCURL(curl, &readBuffer, url, referer, "");

                CURLcode res = curl_easy_perform(curl);
                curl_slist_free_all(headers);

                if (res != CURLE_OK)
                    cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << endl;

                //auto start = std::chrono::steady_clock::now(); // 여기 최적화 할 예정 <------

                //vector<string> blogIds;
                //int collectCnt = 0;

                regex sympathyBlogIdRegex(R"regex("domainIdOrBlogId":"(.*?)")regex");
                smatch match;

                auto begin = sregex_iterator(readBuffer.begin(), readBuffer.end(), sympathyBlogIdRegex);
                auto end = sregex_iterator();

                vector<string> blogIds;
                int collectCnt = 0;
                cout << "Collect Sympathy Blogger Ids\n";
                for (auto j = begin; j != end; ++j) {
                    string id = "N" + (*j)[1].str();
                    if (!ENABLE_DB_UPLOAD) {
                        if (visited.find(id) == visited.end()) {
                            blogIds.push_back(id);
                            visited.insert({ id, true });
                            cout << "Current Collect : " << ++collectCnt << "\r";
                        }
                    }
                    else if (CheckLinkNotVisited(curl, id)) {
                        blogIds.push_back(id);
                        cout << "Current Collect : " << ++collectCnt << "\r";
                    }
                }
                cout << "\n";

                /*auto end_ = std::chrono::steady_clock::now();

                auto duration = end_ - start;

                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
                cout << "duration : " << ms << "ms\n";*/

                if (blogIds.empty()) {
                    Delay(DELAY_MILLI_N, "main");
                    continue;
                }

                vector<bool> registerChecker(blogIds.size(), true);

                if (ENABLE_DB_UPLOAD) {
                    for (int i = 0; i < blogIds.size(); i++) {
                        registerChecker[i] = RegisterLink(curl, blogIds[i]);
                    }
                }

                Publish(*blogProfilePublisher, blogIds, registerChecker);
                Delay(DELAY_MILLI_N, "main");
            }
            else if (link[0] == 'T') {
                string url = "https://" + profileName + ".tistory.com/m/api/" + writingNumber + "/comment";

                if (!IsAllowedByRobotsGeneral(url)) {
                    cout << "SKIP: Robots.txt denied access for [" << link << "] URL [" << url << "]\\n";
                    Delay(DELAY_MILLI_T, "main");
                    continue;
                }

                struct curl_slist* headers = SetCURL(curl, &readBuffer, url);

                CURLcode res = curl_easy_perform(curl);
                curl_slist_free_all(headers);

                if (res != CURLE_OK)
                    cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << endl;

                regex commentBlogHomepageRegex("\"homepage\"\\s*:\\s*\"https://([^\"/]*)");
                smatch match;

                auto begin = sregex_iterator(readBuffer.begin(), readBuffer.end(), commentBlogHomepageRegex);
                auto end = sregex_iterator();

                vector<string> blogHomepages;
                int collectCnt = 0;
                cout << "Collect Comment Blogger Homepages\n";
                for (auto j = begin; j != end; ++j) {
                    string full = (*j)[1].str();
                    if (full.find(".tistory.com") == string::npos) {
                        continue;
                    }
                    regex commentBlogHomepageRegex(R"(^([^.]+))");
                    smatch matchId;
                    if (regex_search(full, matchId, commentBlogHomepageRegex)) {
                        string id = "T" + matchId[1].str();
                        if (!ENABLE_DB_UPLOAD) {
                            if (visited.find(id) == visited.end()) {
                                blogHomepages.push_back(id);
                                visited.insert({ id, true });
                                cout << "Current Collect : " << ++collectCnt << "\r";
                            }
                        }
                        else if (CheckLinkNotVisited(curl, id)) {
                            blogHomepages.push_back(id);
                            cout << "Current Collect : " << ++collectCnt << "\r";
                        }
                    }
                }
                cout << "\n";

                if (blogHomepages.empty()) {
                    Delay(DELAY_MILLI_T, "main");
                    continue;
                }

                vector<bool> registerChecker(blogHomepages.size(), true);

                if (ENABLE_DB_UPLOAD) {
                    for (int i = 0; i < blogHomepages.size(); i++) {
                        registerChecker[i] = RegisterLink(curl, blogHomepages[i]);
                    }
                }

                Publish(*blogProfilePublisher, blogHomepages, registerChecker);
                Delay(DELAY_MILLI_T, "main");
            }

            cout << "\n";
        }

        if (curl) {
            curl_easy_cleanup(curl);
        }
    }

    curl_global_cleanup();

    return 0;
}
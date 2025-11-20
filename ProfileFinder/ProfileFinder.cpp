#include "../Library/Library.cpp"

using namespace std;
namespace pubsub = ::google::cloud::pubsub;

const int CRAWL_PER_SECOND = CRAWL_PER_SECOND_MAP.at("ProfileFinder");
const int DELAY_MILLI = 1000 / CRAWL_PER_SECOND;

unique_ptr<pubsub::Publisher> blogProfilePublisher;
unique_ptr<pubsub::Publisher> blogWritingPublisher;

unique_ptr<pubsub::Subscriber> blogProfileSubscriber;
unique_ptr<pubsub::Subscriber> blogWritingLinkForProfileSubscriber;
unique_ptr<pubsub::Subscriber> blogWritingLinkForContentSubscriber;


int main() {
    cin.tie(NULL);
    ios::sync_with_stdio(false);

    blogProfilePublisher = make_unique<pubsub::Publisher>(pubsub::Publisher(pubsub::MakePublisherConnection(pubsub::Topic(PROJECT_ID, PROFILE_TOPIC_ID), google::cloud::Options{}.set<pubsub::MessageOrderingOption>(true))));
    blogWritingPublisher = make_unique<pubsub::Publisher>(pubsub::Publisher(pubsub::MakePublisherConnection(pubsub::Topic(PROJECT_ID, WRITING_TOPIC_ID), google::cloud::Options{}.set<pubsub::MessageOrderingOption>(true))));

    blogProfileSubscriber = make_unique<pubsub::Subscriber>(pubsub::Subscriber(pubsub::MakeSubscriberConnection(pubsub::Subscription(PROJECT_ID, PROFILE_SUB_ID))));
    blogWritingLinkForProfileSubscriber = make_unique<pubsub::Subscriber>(pubsub::Subscriber(pubsub::MakeSubscriberConnection(pubsub::Subscription(PROJECT_ID, WRITING_FOR_PROFILE_SUB_ID))));
    blogWritingLinkForContentSubscriber = make_unique<pubsub::Subscriber>(pubsub::Subscriber(pubsub::MakeSubscriberConnection(pubsub::Subscription(PROJECT_ID, WRITING_FOR_CONTENT_SUB_ID))));

    //Publish(*blogWritingPublisher, { "Nhaesung_88/223597388359" }, "test");
    //Publish(*blogWritingPublisher, { "Tlsas4565/8838853" }, "test");
    //Publish(*blogWritingPublisher, { "Tlsas4565/8838853" }, "test");
    //Publish(*blogWritingPublisher, { "Tnelastory/35" }, "test");
    //Publish(*blogWritingPublisher, { "Tnelastory/37" }, "test");

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    map<string, bool> visited;

    while (true) {
        vector<string> links = Subscribe(*blogWritingLinkForProfileSubscriber, 10);

        if (links[0] == "/TIMEOUTED") {
            continue;
        }

        CURL* curl;
        string readBuffer;

        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl = curl_easy_init();
        if (curl) {
            for (int i = 0; i < links.size(); i++) {
                string link = links[i];
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
                        Delay(DELAY_MILLI);
                        continue;
                    }

                    struct curl_slist* headers = SetCURL(curl, &readBuffer, url, referer, "", true);

                    CURLcode res = curl_easy_perform(curl);
                    curl_slist_free_all(headers);

                    if (res != CURLE_OK)
                        cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << endl;

                    regex sympathyBlogIdRegex(R"regex("domainIdOrBlogId":"(.*?)")regex");
                    smatch match;

                    auto begin = sregex_iterator(readBuffer.begin(), readBuffer.end(), sympathyBlogIdRegex);
                    auto end = sregex_iterator();

                    vector<string> blogIds;
                    int collectCnt = 0;
                    cout << "Collect Sympathy Blogger Ids\n";
                    for (auto j = begin; j != end; ++j) {
                        string id = "N" + (*j)[1].str();
                        if (visited.find(id) == visited.end()) {
                            blogIds.push_back(id);
                            visited.insert({ id, true });
                            cout << "Current Collect : " << ++collectCnt << "\r";
                        }
                    }
                    cout << "\n";

                    if (blogIds.empty()) continue;

                    Publish(*blogProfilePublisher, blogIds, "test");
                }
                else if (link[0] == 'T') {
                    string url = "https://" + profileName + ".tistory.com/m/api/" + writingNumber + "/comment";

                    if (!IsAllowedByRobotsGeneral(url)) {
                        cout << "SKIP: Robots.txt denied access for [" << link << "] URL [" << url << "]\\n";
                        Delay(DELAY_MILLI);
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
                            if (visited.find(id) == visited.end()) {
                                blogHomepages.push_back(id);
                                visited.insert({ id, true });
                                cout << "Current Collect : " << ++collectCnt << "\r";
                            }
                        }
                    }
                    cout << "\n";

                    if (blogHomepages.empty()) continue;

                    Publish(*blogProfilePublisher, blogHomepages, "test");
                }

                Delay(DELAY_MILLI);
                cout << "\n";
            }
        }

        curl_global_cleanup();
    }

    
    return 0;
}
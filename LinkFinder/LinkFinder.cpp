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


int main() {
    cin.tie(NULL);
    ios::sync_with_stdio(false);

    blogProfilePublisher = make_unique<pubsub::Publisher>(pubsub::Publisher(pubsub::MakePublisherConnection(pubsub::Topic(PROJECT_ID, PROFILE_TOPIC_ID), google::cloud::Options{}.set<pubsub::MessageOrderingOption>(true))));
    blogWritingPublisher = make_unique<pubsub::Publisher>(pubsub::Publisher(pubsub::MakePublisherConnection(pubsub::Topic(PROJECT_ID, WRITING_TOPIC_ID), google::cloud::Options{}.set<pubsub::MessageOrderingOption>(true))));

    blogProfileSubscriber = make_unique<pubsub::Subscriber>(pubsub::Subscriber(pubsub::MakeSubscriberConnection(pubsub::Subscription(PROJECT_ID, PROFILE_SUB_ID))));
    blogWritingLinkForProfileSubscriber = make_unique<pubsub::Subscriber>(pubsub::Subscriber(pubsub::MakeSubscriberConnection(pubsub::Subscription(PROJECT_ID, WRITING_FOR_PROFILE_SUB_ID))));
    blogWritingLinkForContentSubscriber = make_unique<pubsub::Subscriber>(pubsub::Subscriber(pubsub::MakeSubscriberConnection(pubsub::Subscription(PROJECT_ID, WRITING_FOR_CONTENT_SUB_ID))));

    //Publish(*blogProfilePublisher, { "Nhaesung_88" }, ORDERING_KEY);
    //Publish(*blogProfilePublisher, { "Tnelastory" }, ORDERING_KEY);
    //Publish(*blogProfilePublisher, { "N1_do_everything" }, ORDERING_KEY);
    //Publish(*blogProfilePublisher, { "Tmungdenson" }, ORDERING_KEY);

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    while (true) {
        vector<string> links = Subscribe(*blogProfileSubscriber, 10);

        if (links[0] == TIMEOUTED) {
            continue;
        }

        string readBuffer;
        CURL* curl;

        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl = curl_easy_init();
        if (curl) {
            for (int i = 0; i < links.size(); i++) {
                string link = links[i];
                if (link == "") {
                    break;
                }

                if (link[0] == 'N') {
                    string blogName = link.substr(1);
                    vector<string> validPages;
                    int collectCnt = 0;
                    int currentPage = 1;

                    set<string> foundPostIds;
                    bool duplicateFound = false;

                    while (true) {
                        string url = "https://blog.naver.com/PostTitleListAsync.naver?blogId=" + blogName + "&currentPage=" + to_string(currentPage) + "&countPerPage=30";
                        string referer = "Referer: https://blog.naver.com/" + blogName;

                        if (!IsAllowedByRobotsGeneral(url)) {
                            cout << "SKIP: Robots.txt denied access for [" << link << "] URL [" << url << "]\\n";
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

                        regex logNoRegex(R"D("logNo":"(\d+)")D");
                        smatch matchLogNo;
                        string::const_iterator searchStart(readBuffer.cbegin());
                        int pagesFoundInThisCall = 0;

                        while (regex_search(searchStart, readBuffer.cend(), matchLogNo, logNoRegex)) {
                            string postId = matchLogNo[1].str();

                            if (foundPostIds.count(postId)) {
                                duplicateFound = true;
                                break;
                            }

                            foundPostIds.insert(postId);
                            validPages.push_back("N" + blogName + "/" + postId);

                            searchStart = matchLogNo.suffix().first;
                            collectCnt++;
                            pagesFoundInThisCall++;
                        }

                        cout << "Current Collect : " << collectCnt << " (Page: " << currentPage << ")\r";

                        if (duplicateFound || pagesFoundInThisCall == 0) {
                            break;
                        }

                        currentPage++;
                        Delay(DELAY_MILLI_N);
                    }

                    printf("\n");
                    Publish(*blogWritingPublisher, validPages, ORDERING_KEY);
                    Delay(DELAY_MILLI_N);
                }
                else if (link[0] == 'T') {
                    string url = "https://" + link.substr(1) + ".tistory.com/rss";

                    if (!IsAllowedByRobotsGeneral(url)) {
                        cout << "SKIP: Robots.txt denied access for [" << link << "] URL [" << url << "]\\n";
                        Delay(DELAY_MILLI_T);
                        continue;
                    }

                    struct curl_slist* headers = SetCURL(curl, &readBuffer, url);

                    CURLcode res = curl_easy_perform(curl);
                    curl_slist_free_all(headers);
                    if (res != CURLE_OK)
                        cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << endl;

                    regex linkRegex("<link>https://[^/]+/([0-9]+)</link>");
                    smatch match;

                    if (regex_search(readBuffer, match, linkRegex)) {
                        try {
                            int max = stoi(match[1].str());
                            vector<string> validPages;

                            CURLM* multi_handle = curl_multi_init();
                            vector<CURL*> easy_handles;
                            vector<struct curl_slist*> headersList;
                            int activeTransfers = 0;
                            int completed = 0;
                            int emptyPageCnt = 0;

                            cout << "Requests (total: " << max << ")\n";

                            int currentIndex = max;
                            for (int j = 0; j < MAX_CONCURRENT_REQUESTS && currentIndex > 0; ++j) {
                                CURL* eh = curl_easy_init();
                                string* buffer = new string();
                                string url = "https://" + link.substr(1) + ".tistory.com/" + to_string(currentIndex);

                                if (!IsAllowedByRobotsGeneral(url)) {
                                    cout << "SKIP: Robots.txt denied access for [" << link << "] URL [" << url << "]\\n";
                                    Delay(DELAY_MILLI_T);
                                    currentIndex--;
                                    continue;
                                }

                                headersList.push_back(SetCURL(eh, buffer, url, "", "0-256"));
                                curl_multi_add_handle(multi_handle, eh);
                                easy_handles.push_back(eh);
                                activeTransfers++;
                                currentIndex--;
                            }

                            int running = 0;
                            curl_multi_perform(multi_handle, &running);

                            while (running) {
                                int numfds = 0;
                                CURLMcode mc = curl_multi_wait(multi_handle, NULL, 0, 1000, &numfds);
                                if (mc != CURLM_OK) continue;
                                curl_multi_perform(multi_handle, &running);

                                CURLMsg* msg;
                                int msgs_left;
                                while ((msg = curl_multi_info_read(multi_handle, &msgs_left))) {
                                    if (msg->msg == CURLMSG_DONE) {
                                        CURL* eh = msg->easy_handle;
                                        string* buffer;
                                        curl_easy_getinfo(eh, CURLINFO_PRIVATE, &buffer);

                                        smatch matchTitle;
                                        regex titleRegex("<title>(.*?)</title>");
                                        if (regex_search(*buffer, matchTitle, titleRegex)) {
                                            if (matchTitle[1].str() != "TISTORY") {
                                                emptyPageCnt = 0;
                                                char* url;
                                                curl_easy_getinfo(eh, CURLINFO_EFFECTIVE_URL, &url);

                                                string urlStr(url);
                                                smatch idMatch;
                                                regex idRe(".*/([0-9]+)$");

                                                if (regex_search(urlStr, idMatch, idRe)) {
                                                    validPages.push_back("T" + link.substr(1) + "/" + idMatch[1].str());
                                                }
                                            }
                                            else {
                                                emptyPageCnt++;
                                                if (emptyPageCnt >= 20) {
                                                    currentIndex = 0;
                                                }
                                            }
                                        }

                                        delete buffer;
                                        curl_multi_remove_handle(multi_handle, eh);
                                        curl_easy_cleanup(eh);
                                        --activeTransfers;
                                        ++completed;
                                        PrintProgressBar(completed, max);

                                        if (currentIndex > 0) {
                                            CURL* eh = curl_easy_init();
                                            string* buffer = new string();
                                            string url = "https://" + link.substr(1) + ".tistory.com/" + to_string(currentIndex);

                                            if (!IsAllowedByRobotsGeneral(url)) {
                                                cout << "SKIP: Robots.txt denied access for [" << link << "] URL [" << url << "]\\n";
                                                Delay(DELAY_MILLI_T);
                                                currentIndex--;
                                                continue;
                                            }

                                            headersList.push_back(SetCURL(eh, buffer, url, "", "0-256"));
                                            curl_multi_add_handle(multi_handle, eh);
                                            activeTransfers++;
                                            currentIndex--;

                                            Delay(DELAY_MILLI_T);
                                        }
                                    }
                                }
                            }

                            if (!headersList.empty()) {
                                for (int j = 0; j < headersList.size(); j++) {
                                    curl_slist_free_all(headersList[j]);
                                }
                                headersList.clear();
                            }

                            cout << "\n# Valid Page Count : " << validPages.size() << endl;
                            curl_multi_cleanup(multi_handle);

                            Publish(*blogWritingPublisher, validPages, ORDERING_KEY);
                        }
                        catch (exception& e) {
                            cout << "I hate Tistory" << endl;
                        }
                    }
                }
                cout << "\n";
            }
        }
        curl_global_cleanup();
    }



    return 0;
}
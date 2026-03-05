#include "../Library/Library.cpp"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

const int CRAWL_PER_SECOND_N = CRAWL_PER_SECOND_MAP.at("ProfileFinder_N");
const int CRAWL_PER_SECOND_T = CRAWL_PER_SECOND_MAP.at("ProfileFinder_T");
const int DELAY_MILLI_N = 1000 / CRAWL_PER_SECOND_N;
const int DELAY_MILLI_T = 1000 / CRAWL_PER_SECOND_T;

queue<Message> messageQueue;
queue<int> deleteQueue;


int main() {
    cin.tie(NULL);
    ios::sync_with_stdio(false);

    //Nhaesung_88/223597388359
    //Tlsas4565/8838853
    //Tlsas4565/8838853
    //Tnelastory/35
    //Tnelastory/37

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL* curl;

    thread linkFinderSubscribeThread(GetQueue, "profile", &messageQueue);
    linkFinderSubscribeThread.detach();
    thread linkFinderDeleteThread(DeleteQueue, "profile", &deleteQueue);
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
        if (message.isLocked() || link.empty()) {
            continue;
        }

        {
            lock_guard<mutex> lock(deleteQueueMutex);
            deleteQueue.push(message.id);
        }

        string readBuffer;

        curl = curl_easy_init();
        if (curl) {
            int slashIndex = link.find('/');
            string profileName = link.substr(1, slashIndex - 1);
            string writingNumber = link.substr(slashIndex + 1);

            if (link[0] == 'N') {
                string url = "https://blog.naver.com/api/blogs/" + profileName + "/posts/" + writingNumber + "/sympathy-users?itemCount=100&timeStamp=9999999999999";
                string referer = "https://blog.naver.com/SympathyHistoryList.naver?blogId=" + profileName + "&logNo=" + writingNumber;

                if (!IsAllowedByRobotsGeneral(url)) {
                    cout << "SKIP: Robots.txt denied access for [" + link + "] URL [" + url + "]\n";
                    Delay(DELAY_MILLI_N, "main");
                    continue;
                }

                struct curl_slist* headers = SetCURL(curl, &readBuffer, url, referer, "");

                CURLcode res = curl_easy_perform(curl);
                curl_slist_free_all(headers);

                if (res != CURLE_OK) {
                    string str_t(curl_easy_strerror(res));
                    cerr << "curl_easy_perform() failed: " + str_t + "\n";
                }

                regex sympathyBlogIdRegex(R"regex("domainIdOrBlogId":"(.*?)")regex");
                smatch match;

                auto begin = sregex_iterator(readBuffer.begin(), readBuffer.end(), sympathyBlogIdRegex);
                auto end = sregex_iterator();

                vector<string> blogIds;
                int collectCnt = 0;
                cout << "Collect Sympathy Blogger Ids\n";
                for (auto j = begin; j != end; ++j) {
                    string id = "N" + (*j)[1].str();
                    if (CheckLinkNotVisited(curl, id)) {
                        blogIds.push_back(id);
                        cout << "Current Collect : " + to_string(++collectCnt) + "\r";
                    }
                }
                cout << "\n";

                if (blogIds.empty()) {
                    Delay(DELAY_MILLI_N, "main");
                    continue;
                }

                vector<bool> registerChecker(blogIds.size(), true);

                for (int i = 0; i < blogIds.size(); i++) {
                    registerChecker[i] = RegisterLink(curl, blogIds[i]);
                }

                PostQueue("user", blogIds, registerChecker);
                Delay(DELAY_MILLI_N, "main");
            }
            else if (link[0] == 'T') {
                string url = "https://" + profileName + ".tistory.com/m/api/" + writingNumber + "/comment";

                if (!IsAllowedByRobotsGeneral(url)) {
                    cout << "SKIP: Robots.txt denied access for [" + link + "] URL [" + url + "]\n";
                    Delay(DELAY_MILLI_T, "main");
                    continue;
                }

                struct curl_slist* headers = SetCURL(curl, &readBuffer, url);

                CURLcode res = curl_easy_perform(curl);
                curl_slist_free_all(headers);

                if (res != CURLE_OK) {
                    string str_t(curl_easy_strerror(res));
                    cerr << "curl_easy_perform() failed: " + str_t + "\n";
                }

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
                        if (CheckLinkNotVisited(curl, id)) {
                            blogHomepages.push_back(id);
                            cout << "Current Collect : " + to_string(++collectCnt) + "\r";
                        }
                    }
                }
                cout << "\n";

                if (blogHomepages.empty()) {
                    Delay(DELAY_MILLI_T, "main");
                    continue;
                }

                vector<bool> registerChecker(blogHomepages.size(), true);

                for (int i = 0; i < blogHomepages.size(); i++) {
                    registerChecker[i] = RegisterLink(curl, blogHomepages[i]);
                }

                PostQueue("user", blogHomepages, registerChecker);
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
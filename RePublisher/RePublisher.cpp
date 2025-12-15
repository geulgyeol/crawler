#include "../Library/Library.cpp"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;
namespace pubsub = ::google::cloud::pubsub;
namespace fs = filesystem;

unique_ptr<pubsub::Publisher> blogWritingPublisher;

string path = "C:\\Users\\k5517\\Downloads\\html-storage-main\\html-storage-main\\data\\2025\\12\\15";
int maxWritingsSize = 1000;
CURL* curl;
vector<string> blogWritings;

void RePublish() {
    vector<string> blogWritingsForRegister;
    for (int i = 0; i < blogWritings.size(); i++) {
        string message_t = blogWritings[i];
        size_t pos = message_t.find('/');
        if (pos != string::npos) {
            message_t.replace(pos, 1, "%20");
        }
        blogWritingsForRegister.push_back("ReSubscriber_" + message_t);
    }
    vector<bool> checker = RegisterLinks(curl, blogWritingsForRegister);
    Publish(*blogWritingPublisher, blogWritings, checker);

    for (int i = 0; i < checker.size(); i++) {
        if (checker[i]) {
            string message_t = blogWritings[i];
            size_t pos = message_t.find('/');
            if (pos != string::npos) {
                message_t.replace(pos, 1, "%20");
            }
            DeleteFromStorage(curl, "Crawler_" + message_t, "kv");
        }
    }

    vector<string> retry;
    for (int i = 0; i < checker.size(); i++) {
        string message_t = blogWritings[i];
        size_t pos = message_t.find('/');
        if (pos != string::npos) {
            message_t.replace(pos, 1, "%20");
        }
        if (!checker[i] && CheckLinkNotVisited(curl, "ReSubscriber_" + message_t)) {
            retry.push_back(blogWritings[i]);
        }
    }

    blogWritings.clear();

    if (!retry.empty()) {
        for (int i = 0; i < retry.size(); i++) {
            blogWritings.push_back(retry[i]);
        }
    }
}

int main() {
    cin.tie(NULL);
    ios::sync_with_stdio(false);

    blogWritingPublisher = make_unique<pubsub::Publisher>(pubsub::Publisher(pubsub::MakePublisherConnection(pubsub::Topic(PROJECT_ID, WRITING_TOPIC_ID), google::cloud::Options{}.set<pubsub::MessageOrderingOption>(true))));

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    curl_global_init(CURL_GLOBAL_DEFAULT);

    curl = curl_easy_init();
    
    try {
        for (const auto& entry : fs::directory_iterator(path)) {
            if (fs::is_directory(entry)) continue;

            string fileName = entry.path().filename().string();
            fileName = fileName.substr(0, fileName.size() - 9);

            int underbarIndex = fileName.find("_");
            int spaceIndex = fileName.find(" ");

            string blogType = fileName.substr(0, underbarIndex);
            string blogger = fileName.substr(underbarIndex + 1, spaceIndex - underbarIndex - 1);
            string writingId = fileName.substr(spaceIndex + 1, fileName.size() - spaceIndex - 1);

            string message = "";
            if (blogType == "naver") {
                message = "N";
            }
            else if (blogType == "tistory") {
                message = "T";
            }
            else {
                continue;
            }

            message.append(blogger + "/" + writingId);

            string message_t = message;
            size_t pos = message_t.find('/');
            if (pos != string::npos) {
                message_t.replace(pos, 1, "%20");
            }

            if (!CheckLinkNotVisited(curl, "ReSubscriber_" + message_t)) continue;

            blogWritings.push_back(message);

            if (!blogWritings.empty() && blogWritings.size() >= maxWritingsSize) {
                RePublish();
            }
        }

        if (!blogWritings.empty()) {
            RePublish();
        }
    }
    catch (const fs::filesystem_error e) {
        std::cerr << "filesystem_error: " << e.what() << endl;
    }


    if (curl) {
        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();

    return 0;
}
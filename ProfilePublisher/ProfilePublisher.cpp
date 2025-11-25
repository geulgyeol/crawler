#include "../Library/Library.cpp"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;
namespace pubsub = ::google::cloud::pubsub;

unique_ptr<pubsub::Publisher> blogProfilePublisher;

int main() {
    cin.tie(NULL);
    ios::sync_with_stdio(false);

    blogProfilePublisher = make_unique<pubsub::Publisher>(pubsub::Publisher(pubsub::MakePublisherConnection(pubsub::Topic(PROJECT_ID, PROFILE_TOPIC_ID), google::cloud::Options{}.set<pubsub::MessageOrderingOption>(true))));

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    curl_global_init(CURL_GLOBAL_DEFAULT);

    CURL* curl;
    curl = curl_easy_init();

    cout << "Input Profile to Publish\n\n";
    cout << "Profile Format : [N or T][Profile Name]\n\n";
    cout << "ex)   Nhello\n";
    cout << "ex)   TWorld\n";
    cout << "ex)   NAbCd T1234_ff TAAA123__bcd\n\n\n\n";

    while (true) {
        string p;
        string line;
        vector<string> profiles;
        
        cout << "Input > ";

        if (!getline(cin, line)) {
            break;
        }

        stringstream ss(line);

        while (ss >> p) {
            profiles.push_back(p);
        }

        vector<pair<int, string>> failed;
        vector<bool> registerChecker(profiles.size(), false);

        for (int i = 0; i < profiles.size(); i++) {
            string profile = profiles[i];

            if (!(profile[0] == 'N' || profile[0] == 'T')) {
                failed.push_back({i, "First Character is Allowed only N or T"});
                continue;
            }

            if (CheckLinkNotVisited(curl, profile)) {
                registerChecker[i] = RegisterLink(curl, profile);
            }
            else {
                failed.push_back({ i, "Failed to Upload link-kv(Already exist or Failed Connect)" });
                continue;
            }
        }

        Publish(*blogProfilePublisher, profiles, ORDERING_KEY, registerChecker);

        cout << "\n";

        for (int i = 0; i < failed.size(); i++) {
            cout << profiles[failed[i].first] << ": " << failed[i].second << "\n";
        }

        cout << "\n";
    }

    if (curl) {
        curl_easy_cleanup();
    }

    curl_global_cleanup();

    return 0;
}
#include "../Library/Library.cpp"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

int main() {
    cin.tie(NULL);
    ios::sync_with_stdio(false);

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    curl_global_init(CURL_GLOBAL_DEFAULT);

    CURL* curl;
    curl = curl_easy_init();

    cout << "Input Queue Name and Payloads to Publish\n\n";
    cout << "Queue Names: user, profile, content\n";
    cout << "Format : [Queue Name] [N or T][Profile Name(/Number)] ...\n\n";
    cout << "ex)   user Nhello\n";
    cout << "ex)   profile TWorld/1233\n";
    cout << "ex)   content NAbCd/1 T1234_ff/33 TAAA123__bcd/612344122\n\n\n\n";

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

        string queueName = profiles[0];
        profiles.erase(profiles.begin() + 0);

        vector<pair<int, string>> failed;
        vector<bool> registerChecker(profiles.size(), false);

        if (queueName != "user" && queueName != "profile" && queueName != "content") {
            cout << "Queue name must be one of user, profile, content.\n";
            continue;
        }

        if (profiles.empty()) {
            cout << "No Payloads\n";
            continue;
        }

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

        PostQueue(queueName, profiles, registerChecker);

        cout << "\n";

        for (int i = 0; i < failed.size(); i++) {
            cout << profiles[failed[i].first] << ": " << failed[i].second << "\n";
        }

        cout << "\n";
    }

    if (curl) {
        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();

    return 0;
}
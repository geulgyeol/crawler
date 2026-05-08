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

    Client client(PULSAR_SERVICE_URL, CreateClientConfig(LOG_LEVEL));

    Producer userProducer;
    Result res1 = CreateProducer(client, &userProducer, "user");
    Producer profileProducer;
    Result res2 = CreateProducer(client, &profileProducer, "profile");
    Producer contentProducer;
    Result res3 = CreateProducer(client, &contentProducer, "content");

    map<string, Producer*> producers;
    producers.insert({ "user", &userProducer });
    producers.insert({ "profile", &profileProducer });
    producers.insert({ "content", &contentProducer });


    curl_global_init(CURL_GLOBAL_DEFAULT);

    CURL* curl;
    curl = curl_easy_init();

    cout << "Input Topic and Messages to Publish\n\n";
    cout << "Topics: user, profile, content\n";
    cout << "Format : [Topic] [N or T][Profile Name(/Number)] ...\n\n";
    cout << "ex)   user Nhello\n";
    cout << "ex)   profile TWorld/1233\n";
    cout << "ex)   content NAbCd/1 T1234_ff/33 TAAA123__bcd/612344122\n\n\n\n";

    while (true) {
        string p;
        string line;
        vector<string> messages;
        
        cout << "Input > ";

        if (!getline(cin, line)) {
            break;
        }

        stringstream ss(line);

        while (ss >> p) {
            messages.push_back(p);
        }

        string topic = messages[0];
        messages.erase(messages.begin() + 0);

        vector<pair<int, string>> failed;
        vector<bool> registerChecker(messages.size(), false);

        if (topic != "user" && topic != "profile" && topic != "content") {
            cout << "Topic must be one of user, profile, content.\n";
            continue;
        }

        if (messages.empty()) {
            cout << "No Messages\n";
            continue;
        }

        for (int i = 0; i < messages.size(); i++) {
            string profile = messages[i];

            if (!(profile[0] == 'N' || profile[0] == 'T')) {
                failed.push_back({i, "First Character is Allowed only N or T"});
                continue;
            }

            if (CheckLinkNotVisited(curl, profile, (topic == "user" ? "users" : "posts"))) {
                registerChecker[i] = true;
                RegisterLink(curl, profile, (topic == "user" ? "users" : "posts"));
            }
            else {
                failed.push_back({ i, "Failed to Send (Already exist or Failed Connect)" });
                continue;
            }
        }

        SendMessages(*producers[topic], messages, registerChecker);

        cout << "\n";

        for (int i = 0; i < failed.size(); i++) {
            cout << messages[failed[i].first] << ": " << failed[i].second << "\n";
        }

        cout << "\n";
    }

    if (curl) {
        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();

    return 0;
}
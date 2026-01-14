#include "../Library/Library.cpp"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;
namespace pubsub = ::google::cloud::pubsub;
namespace fs = filesystem;

unique_ptr<pubsub::Subscriber> blogProfileSubscriber;
unique_ptr<pubsub::Subscriber> blogWritingLinkForProfileSubscriber;
unique_ptr<pubsub::Subscriber> blogWritingLinkForContentSubscriber;

const string outputPath = "C:\\Users\\k5517\\OneDrive\\¹®¼­\\output";

vector<string> subscriberName = { "blog_profile_subscriber",
                                  "blog_writing_link_for_profile_subscriber",
                                  "blog_writing_link_for_content_subscriber" };

void MsgSubscribe(pubsub::Subscriber subscriber, int subscriberIndex);

int main() {
    cin.tie(NULL);
    ios::sync_with_stdio(false);

    blogProfileSubscriber = make_unique<pubsub::Subscriber>(pubsub::Subscriber(pubsub::MakeSubscriberConnection(pubsub::Subscription(PROJECT_ID, PROFILE_SUB_ID))));
    blogWritingLinkForProfileSubscriber = make_unique<pubsub::Subscriber>(pubsub::Subscriber(pubsub::MakeSubscriberConnection(pubsub::Subscription(PROJECT_ID, WRITING_FOR_PROFILE_SUB_ID))));
    blogWritingLinkForContentSubscriber = make_unique<pubsub::Subscriber>(pubsub::Subscriber(pubsub::MakeSubscriberConnection(pubsub::Subscription(PROJECT_ID, WRITING_FOR_CONTENT_SUB_ID))));

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    while (true) {
        for (int i = 0; i < subscriberName.size(); i++) {
            cout << i << ": " << subscriberName[i] << "\n";
        }

        string ans;
        cin >> ans;

        int ansNum = -1;

        try {
            ansNum = stoi(ans);
            if (ansNum < 0 || ansNum >= subscriberName.size()) {
                ansNum = -1;
            }
        }
        catch(exception e){}

        if (ansNum == -1) {
            for (int i = 0; i < subscriberName.size(); i++) {
                if (subscriberName[i] == ans) {
                    ansNum = i;
                }
            }
        }

        if (ansNum == -1) {
            cout << ans << " is invalid\n";
            continue;
        }

        if (ansNum == 0) {
            MsgSubscribe(*blogProfileSubscriber, ansNum);
        }
        else if (ansNum == 1) {
            MsgSubscribe(*blogWritingLinkForProfileSubscriber, ansNum);
        }
        else if (ansNum == 2) {
            MsgSubscribe(*blogWritingLinkForContentSubscriber, ansNum);
        }
    }

    

    return 0;
}

void MsgSubscribe(pubsub::Subscriber subscriber, int subscriberIndex) {
    fs::path dirPath = outputPath + "\\msg";
    if (!fs::exists(dirPath)) {
        fs::create_directories(dirPath);
    }

    fs::path outputDirPath = outputPath + "\\msg\\" + subscriberName[subscriberIndex];
    if (!fs::exists(outputDirPath)) {
        fs::create_directories(outputDirPath);
    }

    fs::path txtPath = outputPath + "\\msg\\" + subscriberName[subscriberIndex] + "\\" + to_string(chrono::duration_cast<chrono::seconds>(chrono::system_clock::now().time_since_epoch()).count()) + ".txt";
    if (fs::exists(txtPath)) {
        cout << txtPath << " already exist\n";
        return;
    }

    std::ofstream fout(txtPath, std::ios::app);
    if (!fout.is_open()) {
        cout << "can't open " << txtPath << "\n";
        return;
    }

    int subscribeCnt = 0;

    auto start = std::chrono::steady_clock::now();
    auto lastSubscription = std::chrono::steady_clock::now();
    int lastSubscribeCnt = 0;

    auto session = subscriber.Subscribe(
        [&](pubsub::Message const& m, pubsub::AckHandler h) {
            move(h).ack();

            string line = m.data() + "\n";
            fout << line;

            subscribeCnt++;

            string receiveMessage = " # Received message: " + m.data() + "\n";
            cout << receiveMessage;

            lastSubscription = std::chrono::steady_clock::now();
        }
    );

    while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - lastSubscription).count() <= 10000) {
        if (subscribeCnt - lastSubscribeCnt >= 1000) {
            fout.flush();
            lastSubscribeCnt = subscribeCnt;
        }
        
        Delay(100, "main");
    }

    session.cancel();
    auto session_status = session.get();
    string log = to_string(subscribeCnt) + " Subscribing in " + GetTakenTime(start) + "\n";
    cout << log;
    cout << "session End, status = " << session_status << "\n";

    Delay(100, "main");

    fout.close();
}
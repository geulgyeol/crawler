#include "../Library/Library.cpp"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;
namespace pubsub = ::google::cloud::pubsub;


int main() {
    cin.tie(NULL);
    ios::sync_with_stdio(false);

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    map<string, bool> visited;

    CURL* curl;
    string readBuffer;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    queue<Message> messageQueue;

    //thread linkFinderSubscribeThread(GetQueue, "content", &messageQueue);
    //linkFinderSubscribeThread.detach();
    //while (true) {}

    if (curl) {
        vector<string> payloads = {"Nhaesung_88","Tnelastory","N1_do_everything","Tmungdenson"};
        PostQueue("user", payloads);

        //GetQueue("content");

        /*vector<int> ids = {11,12,13,14,15,16,17,18,19,20};
        DeleteQueue(ids, "content");*/
    }

    curl_global_cleanup();

    return 0;
}
#include "../Library/Library.cpp"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;
using namespace pulsar;


int main() {
    cin.tie(NULL);
    ios::sync_with_stdio(false);

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    Client client(PULSAR_SERVICE_URL, CreateClientConfig(LOG_LEVEL));

    Producer producer;
    Result res = CreateProducer(client, &producer, "user");

    if (res != ResultOk) {
        std::cerr << "Failed to Create Producer: " << res << std::endl;
        return 0;
    }

    Consumer consumer;
    res = SubscribeConsumer(client, &consumer, "user");
    if (res != ResultOk) {
        std::cerr << "Failed to Subscribe Consumer: " << res << std::endl;
        return 0;
    }

    SendMessages(producer, {"Nhaesung_88", "N1_do_everything", "Tmungdenson"});

    //SendMessages(producer, { "Nhaesung_88" });

    /*while (true) {
        pulsar::Message msg;
        consumer.receive(msg);
        consumer.acknowledge(msg);

        cout << msg.getDataAsString() << endl;
    }*/

    Delay(2000, "aaa");

    client.close();


    /*CURL* curl;
    string readBuffer;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    thread linkFinderSubscribeThread(GetQueue, "content", &messageQueue);
    linkFinderSubscribeThread.detach();
    while (true) {}

    if (curl) {
        
    }

    curl_global_cleanup();*/

    return 0;
}
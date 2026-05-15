#include "../Library/Library.cpp"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

const int CRAWL_PER_SECOND_N = CRAWL_PER_SECOND_MAP.at("ImageDownloader_N");
const int CRAWL_PER_SECOND_T = CRAWL_PER_SECOND_MAP.at("ImageDownloader_T");
const int DELAY_MILLI_N = 1000 / CRAWL_PER_SECOND_N;
const int DELAY_MILLI_T = 1000 / CRAWL_PER_SECOND_T;

//queue<Msg> messageQueue;
queue<string> messageQueue;

int main() {
    cin.tie(NULL);
    ios::sync_with_stdio(false);

    //

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    curl_global_init(CURL_GLOBAL_DEFAULT);

    messageQueue = {};

    while (true) {
        if (messageQueue.empty()) {
            cout << "Queue Empty\n";
            break;
        }

        string message = messageQueue.front();
        messageQueue.pop();


    }

    /*Client client(PULSAR_SERVICE_URL, CreateClientConfig(LOG_LEVEL));

    Producer profileProducer;
    Result res1 = CreateProducer(client, &profileProducer, "profile");

    Producer contentProducer;
    Result res2 = CreateProducer(client, &contentProducer, "content");

    Consumer consumer;
    Result res3 = SubscribeConsumer(client, &consumer, "user");

    if (res1 != ResultOk || res2 != ResultOk || res3 != ResultOk) return 0;

    thread linkFinderSubscribeThread(receiveMessages, consumer, &messageQueue);
    linkFinderSubscribeThread.detach();*/

    /*while (true) {
        bool is_empty;
        bool ack = false;

        {
            lock_guard<mutex> lock(messageQueueMutex);
            is_empty = messageQueue.empty();
        }

        if (is_empty) {
            Delay(100, "main");
            continue;
        }

        Msg msg;
        CURL* curl = nullptr;

        ScopeGuard cleanupGuard = { [&]() {
            if (curl) curl_easy_cleanup(curl);
            if (ack) AckMsg(msg);
            else NackMsg(msg);
        } };

        {
            std::lock_guard<std::mutex> lock(messageQueueMutex);
            msg = messageQueue.front();
            messageQueue.pop();
        }
        string message = msg.message;

        if (message.empty()) {
            ack = true;
            continue;
        }

        string link = message;

        string readBuffer;

        curl = curl_easy_init();
        if (curl) {
            if (link[0] == 'N') {
                
            }
            else if (link[0] == 'T') {
                
            }
            cout << "\n";
        }
    }*/

    /*profileProducer.close();
    contentProducer.close();
    consumer.close();

    client.close();*/

    curl_global_cleanup();

    return 0;
}
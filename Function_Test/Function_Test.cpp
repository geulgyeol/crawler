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

    cout << "CRAWLER_NAME: " << CRAWLER_NAME << endl;
    cout << "USER_AGENT: " << USER_AGENT << endl;
    cout << "LINK_KV_ENDPOINT: " << LINK_KV_ENDPOINT << endl;
    cout << "HTML_STORAGE_ENDPOINT: " << HTML_STORAGE_ENDPOINT << endl;
    cout << "PULSAR_SERVICE_URL: " << PULSAR_SERVICE_URL << endl;
    cout << "PULSAR_NAMESPACE: " << PULSAR_NAMESPACE << endl;
    cout << "MAX_CONCURRENT_REQUESTS: " << MAX_CONCURRENT_REQUESTS << endl;
    cout << "BODIES_THRESHOLD: " << BODIES_THRESHOLD << endl;
    cout << "NAVER_TIMEOUT_WAITING_TIME: " << NAVER_TIMEOUT_WAITING_TIME << endl;
    cout << "CONNECTION_TIMEOUT_SECONDS: " << CONNECTION_TIMEOUT_SECONDS << endl;
    cout << "RESPONSE_TIMEOUT_SECONDS: " << RESPONSE_TIMEOUT_SECONDS << endl;
    cout << "MAX_MESSAGE_QUEUE_SIZE: " << MAX_MESSAGE_QUEUE_SIZE << endl;
    cout << "MAX_BATCHING_MESSAGE_COUNT: " << MAX_BATCHING_MESSAGE_COUNT << endl;
    cout << "MAX_BATCHING_DELAY: " << MAX_BATCHING_DELAY << endl;
    cout << "ROBOTS_CACHE_DURATION_SECONDS: " << ROBOTS_CACHE_DURATION_SECONDS << endl;
    cout << "MAX_ROBOTS_CACHE_SIZE: " << MAX_ROBOTS_CACHE_SIZE << endl;
    cout << "VERBOSE: " << VERBOSE << endl;
    cout << "LOG_LEVEL: " << LOG_LEVEL << endl;

    vector<string> CRAWL_PER_SECOND_MAP_KEYS = {
        "LinkFinder_N",
        "LinkFinder_T",
        "ProfileFinder_N",
        "ProfileFinder_T",
        "HTMLCrawler_N",
        "HTMLCrawler_T",
        "ImageDownloader_N",
        "ImageDownloader_T"
    };

    for (int i = 0; i < CRAWL_PER_SECOND_MAP_KEYS.size(); i++) {
        cout << CRAWL_PER_SECOND_MAP_KEYS[i] << ": " << CRAWL_PER_SECOND_MAP[CRAWL_PER_SECOND_MAP_KEYS[i]] << endl;
    }




    /*Client client(PULSAR_SERVICE_URL, CreateClientConfig(LOG_LEVEL));

    Producer producer;
    Result res = CreateProducer(client, &producer, "user");

    if (res != ResultOk) {
        std::cerr << "Failed to Create Producer: " << res << std::endl;
        return 0;
    }

    Consumer consumer;
    res = SubscribeConsumer(client, &consumer, "content");
    if (res != ResultOk) {
        std::cerr << "Failed to Subscribe Consumer: " << res << std::endl;
        return 0;
    }*/

    //SendMessages(producer, {"Nhaesung_88", "N1_do_everything", "Tmungdenson"});

    //SendMessages(producer, { "Nhaesung_88" });

    /*map<string, bool> visited;
    int cnt = 0;

    while (true) {
        pulsar::Message msg;
        consumer.receive(msg);
        consumer.acknowledge(msg);

        string message = msg.getDataAsString();

        if (visited.find(message) != visited.end()) {
            cout << message << endl;
        }
        else {
            visited.insert({ message, true });
        }

        if (!(++cnt % 100)) {
            cout << cnt << endl;
        }
    }

    Delay(2000, "aaa");

    client.close();*/


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
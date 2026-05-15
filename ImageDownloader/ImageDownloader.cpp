#include "../Library/Library.cpp"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

const int CRAWL_PER_SECOND_N = CRAWL_PER_SECOND_MAP.at("ImageDownloader_N");
const int CRAWL_PER_SECOND_T = CRAWL_PER_SECOND_MAP.at("ImageDownloader_T");
const int DELAY_MILLI_N = 1000 / CRAWL_PER_SECOND_N;
const int DELAY_MILLI_T = 1000 / CRAWL_PER_SECOND_T;

queue<Msg> messageQueue;

size_t WriteData(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

string GetExtensions(string content_type) {
    static map<string, string> mime_map = {
        {"image/jpeg", ".jpg"},
        {"image/png", ".png"},
        {"image/gif", ".gif"},
        {"image/webp", ".webp"},
        {"image/x-icon", ".ico"}
    };

    for (auto const& [mime, ext] : mime_map) {
        if (content_type.find(mime) != string::npos) return ext;
    }
    return ".bin";
}

string RemoveQuery(string& url) {
    int index = url.find('?');
    if (index == string::npos) return url;

    return url.substr(0, index);
}

string RemoveProtocol(string& url) {
    int index = url.find(':');
    if (index == string::npos) return url;

    string result = url.substr(index+1);

    while (result[0] == '/') result.erase(result.begin());

    return result;
}

int main() {
    cin.tie(NULL);
    ios::sync_with_stdio(false);

    //

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    curl_global_init(CURL_GLOBAL_DEFAULT);

    Client client(PULSAR_SERVICE_URL, CreateClientConfig(LOG_LEVEL));

    Consumer consumer;
    Result res1 = SubscribeConsumer(client, &consumer, "image");

    if (res1 != ResultOk) return 0;

    thread ImageDownloaderSubscribeThread(receiveMessages, consumer, &messageQueue);
    ImageDownloaderSubscribeThread.detach();

    CURL* curl;
    FILE* fp;
    CURLcode res;

    while (true) {
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
            string tempFileName = "DownloadedImages/tmp";

            fp = fopen(tempFileName.c_str(), "wb");
            if (fp == NULL) {
                std::cerr << "Can't Open File: " << tempFileName << std::endl;
                curl_easy_cleanup(curl);
                ack = false;
                return 0;
            }

            string readBuffer;
            struct curl_slist* headers = SetCURL(curl, &readBuffer, link);

            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteData);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

            res = curl_easy_perform(curl);

            curl_slist_free_all(headers);
            fclose(fp);

            if (res != CURLE_OK) {
                std::cerr << "Download Failed: " << curl_easy_strerror(res) << std::endl;
                remove(tempFileName.c_str());
                ack = false;
                continue;
            }

            char* ct = NULL;
            curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ct);

            string finalExt = ".bin";
            if (ct) {
                finalExt = GetExtensions(ct);
            }

            string finalFileName = "DownloadedImages/image" + finalExt;

            if (rename(tempFileName.c_str(), finalFileName.c_str())) {
                std::cerr << "Rename Failed: " << tempFileName << " to " << finalFileName << std::endl;
                remove(tempFileName.c_str());
                ack = false;
                continue;
            }

            std::cout << "Download Success: " << finalFileName << std::endl;

            // 업로드하는 코드

            remove(finalFileName.c_str());

            ack = true;
            curl_easy_cleanup(curl);
        }
    }

    consumer.close();

    client.close();

    curl_global_cleanup();

    return 0;
}
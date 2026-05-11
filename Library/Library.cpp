
#include "pch.h"
#include "framework.h"
#include "../Library/config.h"
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <chrono>
#include <curl/curl.h>
#include <pulsar/Client.h>
#include <regex>
#include <vector>
#include <mutex>
#include <future>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <queue>
#include <fstream>
#include <filesystem>
#include <set>


using namespace std;
using namespace pulsar;


Config config = Config();

const string CRAWLER_NAME = config.CRAWLER_NAME;

const string USER_AGENT = config.USER_AGENT;

const string LINK_KV_ENDPOINT = config.LINK_KV_ENDPOINT;
const string HTML_STORAGE_ENDPOINT = config.HTML_STORAGE_ENDPOINT;
const string PULSAR_SERVICE_URL = config.PULSAR_SERVICE_URL;

const string PULSAR_NAMESPACE = config.PULSAR_NAMESPACE;

const int MAX_CONCURRENT_REQUESTS = config.MAX_CONCURRENT_REQUESTS;

const int BODIES_THRESHOLD = config.BODIES_THRESHOLD;

const int NAVER_TIMEOUT_WAITING_TIME = config.NAVER_TIMEOUT_WAITING_TIME;

const long CONNECTION_TIMEOUT_SECONDS = config.CONNECTION_TIMEOUT_SECONDS;
const long RESPONSE_TIMEOUT_SECONDS = config.RESPONSE_TIMEOUT_SECONDS;

const int MAX_MESSAGE_QUEUE_SIZE = config.MAX_MESSAGE_QUEUE_SIZE;

const int MAX_BATCHING_MESSAGE_COUNT = config.MAX_BATCHING_MESSAGE_COUNT;
const int MAX_BATCHING_DELAY = config.MAX_BATCHING_DELAY;

const long long ROBOTS_CACHE_DURATION_SECONDS = config.ROBOTS_CACHE_DURATION_SECONDS;
const size_t MAX_ROBOTS_CACHE_SIZE = config.MAX_ROBOTS_CACHE_SIZE;

const bool VERBOSE = config.VERBOSE;

const Logger::Level LOG_LEVEL = config.LOG_LEVEL;

map<const string, const int> CRAWL_PER_SECOND_MAP = config.CRAWL_PER_SECOND_MAP;

map<string, chrono::steady_clock::time_point> lastTimes;



mutex messageQueueMutex;
mutex lastTimesMutex;


int GetCurTimestamp();

struct RobotsRules {
    string userAgent;
    vector<string> disallowPaths;
};

struct RobotsCacheEntry {
    map<string, RobotsRules> rules;
    chrono::steady_clock::time_point lastUpdated = chrono::steady_clock::now() - chrono::hours(100);
    bool exists = false;

    mutex cacheMutex;

    RobotsCacheEntry(const RobotsCacheEntry&) = delete;
    RobotsCacheEntry& operator=(const RobotsCacheEntry&) = delete;
    RobotsCacheEntry() = default;
};

mutex globalRobotsCacheMapMutex;
map<string, RobotsCacheEntry> robotsCacheMap;

struct RequestData {
    string link;
    string body;
    string readBuffer;
    struct curl_slist* headers;
    int index;

    ~RequestData() {
        if (headers) {
            curl_slist_free_all(headers);
        }
    }

    RequestData(RequestData&& other) noexcept
        : link(std::move(other.link)),
        body(std::move(other.body)),
        readBuffer(std::move(other.readBuffer)),
        headers(other.headers) {
        other.headers = nullptr;
    }

    RequestData() : headers(nullptr) {}

    RequestData(const RequestData&) = delete;
    RequestData& operator=(const RequestData&) = delete;
};


struct Msg {
    string message;
    pulsar::Message msg;
    Consumer* consumer;

    Msg(){}

    Msg(pulsar::Message& _msg, Consumer* _consumer) {
        message = _msg.getDataAsString();
        msg = _msg;
        consumer = _consumer;
    }

    ~Msg() {
        //delete msg;
    }
};


struct ScopeGuard {
    function<void()> onExit;
    ~ScopeGuard() { if (onExit) onExit(); }
};


size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
bool IsAllowedByRobotsGeneral(const string& fullUrl);
string GetTakenTime(std::chrono::steady_clock::time_point start);

void Delay(int milliseconds, string thread) {
    using clock = std::chrono::steady_clock;

    clock::time_point last;
    {
        std::lock_guard<std::mutex> lock(lastTimesMutex);
        auto it = lastTimes.find(thread);
        if (it == lastTimes.end()) {
            it = lastTimes.emplace(thread, clock::now()).first;
        }
        last = it->second;
    }

    auto target = last + std::chrono::milliseconds(milliseconds);
    std::this_thread::sleep_until(target);

    {
        std::lock_guard<std::mutex> lock(lastTimesMutex);
        lastTimes[thread] = clock::now();
    }
}

void Delay(char blogType, const int DELAY_MILLI_N, const int DELAY_MILLI_T, string thread) {
    if (blogType == 'N') {
        Delay(DELAY_MILLI_N, thread);
    }
    else if (blogType == 'T') {
        Delay(DELAY_MILLI_T, thread);
    }
    else {
        Delay(max(DELAY_MILLI_N, DELAY_MILLI_T), thread);
    }
}

struct curl_slist* SetCURL(CURL* curl, string* readBuffer, string url, string referer = "", string range = "", string request = "") {
    if (readBuffer) {
        readBuffer->clear();
    }

    struct curl_slist* headers = NULL;

    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, readBuffer);
    curl_easy_setopt(curl, CURLOPT_PRIVATE, readBuffer);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, CONNECTION_TIMEOUT_SECONDS);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, RESPONSE_TIMEOUT_SECONDS);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    if (VERBOSE) curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    headers = curl_slist_append(headers, USER_AGENT.c_str());
    headers = curl_slist_append(headers, "X-Requested-With: XMLHttpRequest");

    if (referer != "") {
        string refererHeader = "Referer: " + referer;
        headers = curl_slist_append(headers, refererHeader.c_str());
    }

    if (range != "") {
        curl_easy_setopt(curl, CURLOPT_RANGE, range.c_str());
    }

    if (request != "") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    return headers;
}

struct curl_slist* SetCurlWithRequest(CURL* curl, string* readBuffer, string url, string request = "", string payload = "") {
    if (readBuffer) {
        readBuffer->clear();
    }

    struct curl_slist* headers = NULL;


    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, readBuffer);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, CONNECTION_TIMEOUT_SECONDS);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, RESPONSE_TIMEOUT_SECONDS);
    if (VERBOSE) curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    headers = curl_slist_append(headers, USER_AGENT.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    if (request != "") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request.c_str());
    }

    if (payload != "") {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, payload.length());
    }

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    return headers;
}


ClientConfiguration CreateClientConfig(Logger::Level logLevel = Logger::Level::LEVEL_ERROR) {
    ClientConfiguration config;
    config.setLogger(new ConsoleLoggerFactory(logLevel));
    config.setTlsAllowInsecureConnection(true);
    config.setValidateHostName(false);

    return config;
}

Result CreateProducer(Client client, Producer* producer, string topic) {
    ProducerConfiguration prop;
    prop.setBatchingEnabled(true);
    prop.setBatchingMaxMessages(MAX_BATCHING_MESSAGE_COUNT);
    prop.setBatchingMaxPublishDelayMs(MAX_BATCHING_DELAY);
    prop.setBlockIfQueueFull(true);

    Result res = client.createProducer(PULSAR_NAMESPACE + topic, prop, *producer);

    if (res != ResultOk) {
        cout << "Failed to Create " + topic + " Producer\n";
    }
    else {
        cout << "Success to Create " + topic + " Producer\n";
    }

    return res;
}

Result SubscribeConsumer(Client client, Consumer* consumer, string topic) {
    ConsumerConfiguration consumerConfig;
    consumerConfig.setConsumerType(pulsar::ConsumerType::ConsumerShared);

    Result res = client.subscribe(PULSAR_NAMESPACE + topic, topic + "-sub", consumerConfig, *consumer);

    if (res != ResultOk) {
        cout << "Failed to Subscribe " + topic + " Consumer\n";
    }
    else {
        cout << "Success to Subscribe " + topic + " Consumer\n";
    }

    return res;
}



void SendMessages(Producer producer, const vector<string>& messages, vector<bool> registerChecker = {}) {
    if (messages.empty()) return;

    int checkerSize = (registerChecker.empty() ? 0 : registerChecker.size());

    for (int i = 0; i < messages.size(); i++) {
        string message = messages[i];
        if (checkerSize > i && !registerChecker[i]) continue;

        pulsar::Message msg = MessageBuilder().setContent(string(message)).build();

        producer.sendAsync(msg, [](Result result, const MessageId& id) {
            if (result != ResultOk) {
                std::cerr << "Failed to Send" << std::endl;
            }
        });
    }
}

void receiveMessages(Consumer consumer, queue<Msg>* messageQueue) {
    int count = 0;

    while (true) {
        pulsar::Message msg;

        {
            lock_guard<mutex> lock(messageQueueMutex);
            count = (messageQueue->empty() ? 0 : messageQueue->size());
        }

        if(count < MAX_MESSAGE_QUEUE_SIZE && consumer.receive(msg, 1000) == ResultOk) {
            {
                lock_guard<mutex> lock(messageQueueMutex);
                messageQueue->push(Msg(msg, &consumer));
            }
        }
    }
}

void AckMsg(Msg msg) {
    msg.consumer->acknowledge(msg.msg);
    //cout << "ACK: " << msg.message << "\n";
}

void NackMsg(Msg msg) {
    msg.consumer->negativeAcknowledge(msg.msg);
    //cout << "NACK: " << msg.message << "\n";
}


void PrintProgressBar(int current, int total) {
    cout << "(" + to_string(current) + "/" + to_string(total) + ")" + "\r";
    cout.flush();
}

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

string ExtractDomainRoot(const string fullUrl) {
    if (fullUrl.empty()) return "";

    string url = fullUrl;
    string protocol;

    string::size_type protocolEnd = url.find("://");
    if (protocolEnd != string::npos) {
        protocol = url.substr(0, protocolEnd + 3);
        url = url.substr(protocolEnd + 3);
    }
    else {
        return "";
    }

    string::size_type domainEnd = url.find('/');
    string domain = (domainEnd != string::npos) ? url.substr(0, domainEnd) : url;

    return protocol + domain;
}

string ExtractUrlPath(const string& fullUrl, const string& domainRootUrl) {
    if (fullUrl.size() <= domainRootUrl.size()) {
        return "/";
    }
    return fullUrl.substr(domainRootUrl.size());
}


bool CheckRules(RobotsCacheEntry& entry, const string& userAgent, const string& path) {
    if (!entry.exists) return true;

    vector<string> pathsToCheck;

    if (entry.rules.count(userAgent)) {
        pathsToCheck = entry.rules.at(userAgent).disallowPaths;
    }

    if (pathsToCheck.empty() && entry.rules.count("*")) {
        pathsToCheck = entry.rules.at("*").disallowPaths;
    }

    for (const auto& disallowedPath : pathsToCheck) {
        if (path.size() >= disallowedPath.size() && path.substr(0, disallowedPath.size()) == disallowedPath) {
            return false;
        }
    }
    return true;
}


void RefreshRobotsCache(const string& domainRootUrl, RobotsCacheEntry& cacheEntry) {
    lock_guard<mutex> lock(cacheEntry.cacheMutex);

    auto now = chrono::steady_clock::now();
    auto elapsed = chrono::duration_cast<chrono::seconds>(now - cacheEntry.lastUpdated).count();

    if (elapsed < ROBOTS_CACHE_DURATION_SECONDS) {
        return;
    }

    cout << "Refreshing robots.txt ..\n";

    string robotsUrl = domainRootUrl + "/robots.txt";
    string robotsContent;

    CURL* curl = curl_easy_init();
    if (!curl) return;

    struct curl_slist* headers = SetCURL(curl, &robotsContent, robotsUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    long httpCode = 0;
    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    cacheEntry.lastUpdated = now;
    cacheEntry.rules.clear();

    if (res != CURLE_OK || httpCode >= 400) {
        cacheEntry.exists = false;
        cerr << "Robots.txt fetch failed or 40x for [" + domainRootUrl + "]. Status Code: " + to_string(httpCode) + "\n";
        return;
    }

    cacheEntry.exists = true;
    stringstream ss(robotsContent);
    string line;
    string currentAgent = "*";

    while (getline(ss, line)) {
        size_t commentPos = line.find('#');
        if (commentPos != string::npos) line = line.substr(0, commentPos);
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);

        if (line.empty()) continue;

        if (line.substr(0, 12) == "User-agent: ") {
            currentAgent = line.substr(12);
            cacheEntry.rules[currentAgent];
        }
        else if (line.substr(0, 10) == "Disallow: ") {
            string disallowedPath = line.substr(10);
            if (!disallowedPath.empty()) {
                cacheEntry.rules[currentAgent].disallowPaths.push_back(disallowedPath);
            }
        }
    }
}

bool IsAllowedByRobotsGeneral(const string& fullUrl) {
    if (fullUrl.empty()) return true;
    
    string domainRootUrl = ExtractDomainRoot(fullUrl);
    string path = ExtractUrlPath(fullUrl, domainRootUrl);

    if (domainRootUrl.empty()) return true;

    RobotsCacheEntry* entryPtr;

    {
        lock_guard<mutex> mapLock(globalRobotsCacheMapMutex);

        if (robotsCacheMap.size() >= MAX_ROBOTS_CACHE_SIZE) {
            cout << "Robots cache size (" + to_string(robotsCacheMap.size()) + ") exceeded limit (" + to_string(MAX_ROBOTS_CACHE_SIZE) + "). Clearing all cache entries.\n";
            robotsCacheMap.clear();
        }

        entryPtr = &robotsCacheMap[domainRootUrl];
    }

    RefreshRobotsCache(domainRootUrl, *entryPtr);

    {
        lock_guard<mutex> entryLock(entryPtr->cacheMutex);
        return CheckRules(*entryPtr, CRAWLER_NAME, path);
    }
}

string EscapeQuotes(const string& input) {
    string result;
    result.reserve(input.size() * 1.5);

    size_t segment_start = 0;
    size_t input_len = input.length();

    for (size_t current_pos = 0; current_pos < input_len; ++current_pos) {
        char c = input[current_pos];
        bool is_special = false;

        if (c == '"' || c == '\\' || c == '\n' || c == '\r' || c == '\t' || c == '\b' || c == '\f') {
            is_special = true;
        }
        else if (static_cast<unsigned char>(c) < 0x20) {
            is_special = true;
        }

        if (is_special) {
            if (current_pos > segment_start) {
                result.append(input, segment_start, current_pos - segment_start);
            }

            if (c == '"') {
                result.append("\\\"");
            }
            else if (c == '\\') {
                result.append("\\\\");
            }
            else if (c == '\n') {
                result.append("\\n");
            }
            else if (c == '\r') {
                result.append("\\r");
            }
            else if (c == '\t') {
                result.append("\\t");
            }
            else if (c == '\b') {
                result.append("\\b");
            }
            else if (c == '\f') {
                result.append("\\f");
            }
            else {
                stringstream ss;
                ss << "\\u"
                    << hex << uppercase
                    << setfill('0') << setw(4)
                    << static_cast<unsigned int>(static_cast<unsigned char>(c));
                result.append(ss.str());
            }

            segment_start = current_pos + 1;
        }
    }

    if (input_len > segment_start) {
        result.append(input, segment_start, input_len - segment_start);
    }

    return result;
}





bool CheckLinkNotVisited(CURL* curl, const string link, const string type) {
    string platform = "";

    if (link[0] == 'N') {
        platform = "naverblog";
    }
    else if (link[0] == 'T') {
        platform = "tistory";
    }
    else {
        cout << "link must start with N or T in CheckLinkNotVisited\n";
        return false;
    }

    string url = config.LINK_KV_ENDPOINT + "/" + type + "/" + platform + "/" + link.substr(1);
    string readBuffer;
    struct curl_slist* headers = SetCURL(curl, &readBuffer, url);
    long httpCode = 0;

    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        cerr << "KV GET failed for [" + link + "]: " + curl_easy_strerror(res) + "\n";
        return false;
    }

    return httpCode == 404;
}

bool RegisterLink(CURL* curl, const string link, const string type) {
    string platform = "";

    if (link[0] == 'N') {
        platform = "naverblog";
    }
    else if (link[0] == 'T') {
        platform = "tistory";
    }
    else {
        cout << "link must start with N or T in CheckLinkNotVisited\n";
        return false;
    }

    string content = "{\"blog_platform\":\"" + platform + "\",\"user_id\":\"" + link.substr(1) + "\"}";

    string url = config.LINK_KV_ENDPOINT + "/" + type;
    string readBuffer;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, content.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, content.length());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, config.USER_AGENT.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    long httpCode = 0;
    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        cerr << "KV POST failed for [" + link + "]: " + curl_easy_strerror(res) + "\n";
        return false;
    }

    return httpCode == 201;
}

vector<bool> RegisterLinks(CURL* curl, vector<string> links, const string type) {
    if (links.empty()) return {};

    vector<bool> checker(links.size(), false);

    CURLM * multi_handle = curl_multi_init();
    if (!multi_handle) {
        cerr << "Failed to initialize CURL multi handle.\n";
        return checker;
    }

    map<CURL*, unique_ptr<RequestData>> requests;

    for (int i = 0; i < links.size(); i++) {
        string link = links[i];
        CURL* eh = curl_easy_init();
        if (!eh) {
            cerr << "Failed to initialize CURL easy handle.\n";
            continue;
        }

        auto data = make_unique<RequestData>();
        data->link = link;
        data->index = i;

        string platform = "";

        if (link[0] == 'N') {
            platform = "naverblog";
        }
        else if (link[0] == 'T') {
            platform = "tistory";
        }
        else {
            cout << "link must start with N or T in CheckLinkNotVisited\n";
            return {NULL};
        }

        string content = "{\"blog_platform\":\"" + platform + "\",\"user_id\":\"" + link.substr(1) + "\"}";

        string url = config.LINK_KV_ENDPOINT + "/" + type;
        string readBuffer;

        curl_easy_setopt(eh, CURLOPT_URL, url.c_str());
        curl_easy_setopt(eh, CURLOPT_CUSTOMREQUEST, "POST");
        curl_easy_setopt(eh, CURLOPT_POSTFIELDS, content.c_str());
        curl_easy_setopt(eh, CURLOPT_POSTFIELDSIZE, content.length());
        curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(eh, CURLOPT_WRITEDATA, &readBuffer);

        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, config.USER_AGENT.c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(eh, CURLOPT_HTTPHEADER, headers);

        data->headers = headers;

        curl_multi_add_handle(multi_handle, eh);

        requests[eh] = move(data);
    }

    int still_running = 0;
    curl_multi_perform(multi_handle, &still_running);

    while (still_running) {
        int numfds = 0;
        CURLMcode mc = curl_multi_wait(multi_handle, NULL, 0, 100, &numfds);
        if (mc != CURLM_OK) break;

        curl_multi_perform(multi_handle, &still_running);
    }

    CURLMsg* msg;
    int msgs_left;
    while ((msg = curl_multi_info_read(multi_handle, &msgs_left))) {
        if (msg->msg == CURLMSG_DONE) {
            CURL* eh = msg->easy_handle;

            const auto& data = requests[eh];
            long response_code;
            curl_easy_getinfo(eh, CURLINFO_RESPONSE_CODE, &response_code);

            if (msg->data.result == CURLE_OK && response_code == 201) {
                cout << "KV POST success for [" + data->link + "] (Code: " + to_string(response_code) + ").\n";
                checker[data->index] = true;
            }
            else {
                cerr << "KV POST FAILED for [" + data->link + "] (Code: " + to_string(response_code) + "). Error: " + curl_easy_strerror(msg->data.result) + "\n";
            }

            curl_multi_remove_handle(multi_handle, eh);
            curl_easy_cleanup(eh);

            requests.erase(eh);
        }
    }

    curl_multi_cleanup(multi_handle);

    return checker;
}

void PostHTMLContent(map<string, string>&& bodies) {
    if (bodies.empty()) return;

    auto start = std::chrono::steady_clock::now();

    string readBuffer;

    CURL* curl = curl_easy_init();
    if (curl) {
        string content = "{";

        int bodiesSearchCnt = 0;
        for (const auto& entry : bodies) {
            string link = entry.first;
            string body = entry.second;

            content.append("\"" + link + "\":" + body);
            if (++bodiesSearchCnt != bodies.size()) {
                content.append(",");
            }
        }

        content.append("}");

        string url = config.HTML_STORAGE_ENDPOINT + "/batch";

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, content.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, content.length());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, config.USER_AGENT.c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);


        CURLcode response_code = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        if (response_code == CURLE_OK) {
            cout << to_string(bodies.size()) + " HTML Posting in " + GetTakenTime(start) + "\n";
        }
        else {
            cerr << "HTML POST FAILED.\n";
        }
    }
}

/*bool DeleteFromStorage(CURL* curl, const string link, const string storage) { // kv or html
    string url;
    if (storage == "kv") {
        url = config.LINK_KV_ENDPOINT + "/" + link;
    }
    else if (storage == "html") {
        url = config.HTML_STORAGE_ENDPOINT + "/" + link;
    }
    else {
        return false;
    }

    string readBuffer;

    struct curl_slist* headers = SetCURL(curl, &readBuffer, url, "", "", "DELETE");

    long httpCode = 0;
    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        cerr << "DELETE failed for [" + link + "]: " + curl_easy_strerror(res) + "\n";
        return false;
    }

    return httpCode == 201;
}*/

string GetTakenTime(std::chrono::steady_clock::time_point start) {
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

    auto duration = end - start;

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    string taken = to_string(ms) + "ms";

    return taken;
}

int GetCurTimestamp() {
    return chrono::duration_cast<chrono::seconds>(chrono::system_clock::now().time_since_epoch()).count();
}

#include "pch.h"
#include "framework.h"
#include "../Library/config.h"
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <chrono>
#include <curl/curl.h>
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


Config config = Config();

const string CRAWLER_NAME = config.CRAWLER_NAME;

const string USER_AGENT = config.USER_AGENT;

const string LINK_KV_ENDPOINT = config.LINK_KV_ENDPOINT;
const string HTML_STORAGE_ENDPOINT = config.HTML_STORAGE_ENDPOINT;
const string QUEUE_ENDPOINT = config.QUEUE_ENDPOINT;

const int MAX_CONCURRENT_REQUESTS = config.MAX_CONCURRENT_REQUESTS;

const int BODIES_THRESHOLD = config.BODIES_THRESHOLD;

const int NAVER_TIMEOUT_WAITING_TIME = config.NAVER_TIMEOUT_WAITING_TIME;

const int QUEUE_TIME_LIMIT = config.QUEUE_TIME_LIMIT;

const long CONNECTION_TIMEOUT_SECONDS = config.CONNECTION_TIMEOUT_SECONDS;
const long RESPONSE_TIMEOUT_SECONDS = config.RESPONSE_TIMEOUT_SECONDS;

const int ENABLE_MESSAGE_QUEUE_THRESHOLD = config.ENABLE_MESSAGE_QUEUE_THRESHOLD;
const int DISABLE_MESSAGE_QUEUE_THRESHOLD = config.DISABLE_MESSAGE_QUEUE_THRESHOLD;

const long long ROBOTS_CACHE_DURATION_SECONDS = config.ROBOTS_CACHE_DURATION_SECONDS;
const size_t MAX_ROBOTS_CACHE_SIZE = config.MAX_ROBOTS_CACHE_SIZE;

const bool VERBOSE = config.VERBOSE;

map<const string, const int> CRAWL_PER_SECOND_MAP = config.CRAWL_PER_SECOND_MAP;

map<string, chrono::steady_clock::time_point> lastTimes;



mutex messageQueueMutex;
mutex deleteQueueMutex;
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

struct Message {
    string message;
    int id;
    int timeLimit;

    Message() {
        message = "";
        id = -1;
        timeLimit = -1;
    }

    Message(string message_v, int id_v) {
        message = message_v;
        id = id_v;
        timeLimit = GetCurTimestamp() + QUEUE_TIME_LIMIT;
    }

    bool isLocked() {
        return (timeLimit < GetCurTimestamp());
    }
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

void PostQueue(string queueName, vector<string>& payloads, vector<bool> checker = {}) {
    if (payloads.empty()) return;

    auto start = std::chrono::steady_clock::now();

    string readBuffer;

    CURL* curl = curl_easy_init();
    if (curl) {
        string content = "{\"payloads\":[";

        int payloadSearchCnt = 0;
        int i = 0;
        for (const string& payload : payloads) {
            if (!checker.empty() && checker.size() > i) {
                if (!checker[i]) continue;
            }

            content.append("\"" + payload + "\"");
            if (++payloadSearchCnt != payloads.size()) {
                content.append(",");
            }
        }

        if (content[content.size() - 1] == ',') {
            content.pop_back();
        }

        content.append("]}");

        string url = config.QUEUE_ENDPOINT + "/" + queueName;

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
            cout << to_string(payloads.size()) + " " + queueName + " Queue POST in " + GetTakenTime(start) + "\n";
        }
        else {
            cerr << queueName + " Queue POST FAILED.\n";
        }
    }
}

void GetQueue(string queueName, queue<Message>* messageQueue) {
    string readBuffer;
    bool subscribeEnabled = false;

    CURL* curl = curl_easy_init();
    if (curl) {
        while (true) {
            auto start = std::chrono::steady_clock::now();
            readBuffer = "";

            {
                lock_guard<mutex> lock(messageQueueMutex);
                subscribeEnabled = (messageQueue->empty() || messageQueue->size() < ENABLE_MESSAGE_QUEUE_THRESHOLD);
            }

            if (subscribeEnabled) {
                bool found = false;

                string url = config.QUEUE_ENDPOINT + "/" + queueName;

                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

                struct curl_slist* headers = NULL;
                headers = curl_slist_append(headers, config.USER_AGENT.c_str());
                headers = curl_slist_append(headers, "Content-Type: application/json");
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);


                CURLcode response_code = curl_easy_perform(curl);
                curl_slist_free_all(headers);
                if (response_code == CURLE_OK) {
                    int findPos = 0;
                    bool isSearching = true;

                    while (isSearching) {
                        int idPos = readBuffer.find("\"id\":", findPos);
                        int idEndPos = readBuffer.find(",", findPos);
                        int payloadPos = readBuffer.find("\"payload\":", findPos);
                        int payloadEndPos = readBuffer.find("}", findPos);

                        if (idPos == string::npos) break;

                        int id = stoi(readBuffer.substr(idPos + 5, idEndPos - (idPos + 5)));
                        string payload = readBuffer.substr(payloadPos + 11, payloadEndPos - 1 - (payloadPos + 11));

                        if (readBuffer[payloadEndPos + 1] == ']') {
                            isSearching = false;
                        }

                        findPos = payloadEndPos + 2;

                        {
                            lock_guard<mutex> lock(messageQueueMutex);
                            messageQueue->push(Message(payload, id));
                        }

                        found = true;
                    }
                }
                else {
                    cerr << queueName + " Queue GET FAILED.\n";
                }

                if (found) {
                    cout << queueName + " Queue GET in " + GetTakenTime(start) + "\n";
                }
            }

            Delay(1000, "subscribe");
        }
    }
}

void DeleteQueue(string queueName, queue<int>* deleteQueue) {
    string readBuffer;

    CURL* curl = curl_easy_init();
    if (curl) {
        while (true) {
            auto start = std::chrono::steady_clock::now();
            readBuffer = "";

            bool deleteQueueEmpty = true;
            {
                lock_guard<mutex> lock(deleteQueueMutex);
                deleteQueueEmpty = deleteQueue->empty();
            }

            if (deleteQueueEmpty) {
                Delay(1000, "delete");
                continue;
            }

            string content = "{\"ids\": [";

            int idSearchCnt = 0;
            vector<int> ids;
            int deleteQueueSize = 0;

            {
                lock_guard<mutex> lock(deleteQueueMutex);
                deleteQueueSize = deleteQueue->size();
            }

            for (int i = 0; i < deleteQueueSize; i++) {
                {
                    lock_guard<mutex> lock(deleteQueueMutex);
                    ids.push_back(deleteQueue->front());
                    deleteQueue->pop();
                }
            }

            for (const int& id : ids) {
                content.append(to_string(id));
                if (++idSearchCnt != ids.size()) {
                    content.append(",");
                }
            }

            content.append("]}");

            string url = config.QUEUE_ENDPOINT + "/" + queueName;

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
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

            int http_code = 0;
            if (response_code == CURLE_OK) {
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

                if (http_code == 200) {
                    cout << to_string(ids.size()) + " " + queueName + " Queue DELETE in " + GetTakenTime(start) + "\n";
                }
                else if (http_code == 400) {
                    cout << queueName + " Queue DELETE FAILED. HTTP Code: " + to_string(http_code) + "\n";
                }
            }
            else {
                cerr << queueName + " Queue DELETE FAILED. HTTP Code: " + to_string(http_code) + "\n";
            }

            Delay(1000, "delete");
        }
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





bool CheckLinkNotVisited(CURL* curl, const string link) {
    string url = config.LINK_KV_ENDPOINT + "/" + link;
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

bool RegisterLink(CURL* curl, const string link) {
    string url = config.LINK_KV_ENDPOINT + "/" + link;

    string readBuffer;

    struct curl_slist* headers = SetCURL(curl, &readBuffer, url, "", "", "POST");

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

vector<bool> RegisterLinks(CURL* curl, vector<string> links) {
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

        string url = config.LINK_KV_ENDPOINT + "/" + data->link;

        struct curl_slist* headers = SetCURL(eh, &data->readBuffer, url, "", "", "POST");
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

bool DeleteFromStorage(CURL* curl, const string link, const string storage) { // kv or html
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
}

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
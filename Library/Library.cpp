
#include "pch.h"
#include "framework.h"
#include "google/cloud/pubsub/publisher.h"
#include "google/cloud/pubsub/subscriber.h"
#include "../Library/config.h"
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


using namespace std;
namespace pubsub = ::google::cloud::pubsub;


const string TIMEOUTED = "/TIMEOUTED";

Config config = Config();

const string PROJECT_ID = config.PROJECT_ID;

const string PROFILE_TOPIC_ID = config.PROFILE_TOPIC_ID;
const string WRITING_TOPIC_ID = config.WRITING_TOPIC_ID;

const string PROFILE_SUB_ID = config.PROFILE_SUB_ID;
const string WRITING_FOR_PROFILE_SUB_ID = config.WRITING_FOR_PROFILE_SUB_ID;
const string WRITING_FOR_CONTENT_SUB_ID = config.WRITING_FOR_CONTENT_SUB_ID;

const string CRAWLER_NAME = config.CRAWLER_NAME;

const string USER_AGENT = config.USER_AGENT;

const string LINK_KV_ENDPOINT = config.LINK_KV_ENDPOINT;
const string HTML_STORAGE_ENDPOINT = config.HTML_STORAGE_ENDPOINT;

const int MAX_CONCURRENT_REQUESTS = config.MAX_CONCURRENT_REQUESTS;
const int DEFAULT_SUB_WAITING_TIME = config.DEFAULT_SUB_WAITING_TIME;

const int ENABLE_MESSAGE_QUEUE_THRESHOLD = config.ENABLE_MESSAGE_QUEUE_THRESHOLD;
const int DISABLE_MESSAGE_QUEUE_THRESHOLD = config.DISABLE_MESSAGE_QUEUE_THRESHOLD;

const long long ROBOTS_CACHE_DURATION_SECONDS = config.ROBOTS_CACHE_DURATION_SECONDS;
const size_t MAX_ROBOTS_CACHE_SIZE = config.MAX_ROBOTS_CACHE_SIZE;

const bool ENABLE_DB_UPLOAD = config.ENABLE_DB_UPLOAD;

map<const string, const int> CRAWL_PER_SECOND_MAP = config.CRAWL_PER_SECOND_MAP;

mutex messageQueueMutex;
mutex subscribeEnabledMutex;


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


size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
bool IsAllowedByRobotsGeneral(const string& fullUrl);

void Delay(int milliseconds) {
    this_thread::sleep_for(chrono::milliseconds(milliseconds));
}

void Delay(char blogType, const int DELAY_MILLI_N, const int DELAY_MILLI_T) {
    if (blogType == 'N') {
        Delay(DELAY_MILLI_N);
    }
    else if (blogType == 'T') {
        Delay(DELAY_MILLI_T);
    }
    else {
        Delay(max(DELAY_MILLI_N, DELAY_MILLI_T));
    }
}

void Publish(pubsub::Publisher publisher, vector<string> contents, vector<bool> checker = {}) try {
    vector<google::cloud::future<google::cloud::StatusOr<string>>> futures;

    for (int i = 0; i < contents.size(); i++) {
        if (!checker.empty() && checker.size() > i) {
            if (!checker[i]) continue;
        }

        const string content = contents[i];
        futures.push_back(
            publisher.Publish(
                pubsub::MessageBuilder{}
                .SetData(content)
                .Build()
        ));
    }

    for (int i = 0; i < futures.size(); ++i) {
        auto id = futures[i].get();
        if (!id) {
            cerr << "Publish failed for \"" << contents[i] << "\": "
                << id.status() << "\n";
        }
        else {
            cout << "\"" << contents[i] << "\" published with id=" << *id << "\n";
        }
    }
}
catch (google::cloud::Status const& status) {
    cerr << "google::cloud::Status thrown: " << status << "\n";
}


void Subscribe(pubsub::Subscriber subscriber, queue<string> *messageQueue, bool *subscribeEnabled, const int waitingTime = DEFAULT_SUB_WAITING_TIME) try {
    while (true) {
        {
            lock_guard<mutex> lock(subscribeEnabledMutex);
            if (messageQueue->empty() || messageQueue->size() < ENABLE_MESSAGE_QUEUE_THRESHOLD) {
                *subscribeEnabled = true;
            }
        }

        bool is_enabled;
        {
            lock_guard<mutex> lock(subscribeEnabledMutex);
            is_enabled = *subscribeEnabled;
        }
        
        if (is_enabled) {
            cout << "Listening for messages on subscription" << endl;

            auto session = subscriber.Subscribe(
                [&](pubsub::Message const& m, pubsub::AckHandler h) {
                    move(h).ack();
                    
                    {
                        lock_guard<mutex> lock(messageQueueMutex);
                        messageQueue->push(m.data());
                    }

                    cout << " # Received message: " << m.data() << "\n";

                    try {
                        lock_guard<mutex> lock_q(subscribeEnabledMutex);
                        lock_guard<mutex> lock_e(messageQueueMutex);

                        if (!messageQueue->empty() && messageQueue->size() > DISABLE_MESSAGE_QUEUE_THRESHOLD) {
                            *subscribeEnabled = false;
                        }
                    }
                    catch (exception e) {
                        lock_guard<mutex> lock(subscribeEnabledMutex);
                        *subscribeEnabled = false;
                    }
                }
            );

            bool still_enabled = true;
            while (still_enabled) {
                Delay(100);
                lock_guard<mutex> lock(subscribeEnabledMutex);
                still_enabled = *subscribeEnabled;
            }

            session.cancel();
            auto session_status = session.get();
            cout << "session End, status = " << session_status << "\n";
        }

        Delay(100);
    }
}
catch (google::cloud::Status const& status) {
    cerr << "google::cloud::Status thrown: " << status << "\n";
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
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

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
    int barWidth = 50;
    float progress = (float)current / total;

    cout << "[";
    int pos = barWidth * progress;
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) cout << "=";
        else if (i == pos) cout << ">";
        else cout << " ";
    }
    cout << "] " << int(progress * 100.0) << "% " << "(" << current << "/" << total << ")" << "\r";
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
        cerr << "Robots.txt fetch failed or 40x for [" << domainRootUrl << "]. Status Code: " << httpCode << endl;
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
            cout << "Robots cache size (" << robotsCacheMap.size() << ") exceeded limit (" << MAX_ROBOTS_CACHE_SIZE << "). Clearing all cache entries.\n";
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
        cerr << "KV GET failed for [" << link << "]: " << curl_easy_strerror(res) << endl;
        return false;
    }

    return httpCode == 404;
}

bool RegisterLink(CURL* curl, const string link) {
    string url = config.LINK_KV_ENDPOINT + "/" + link;
    
    /*if (!CheckLinkNotVisited(curl, link)) {
        cout << "Register Links failed for [" << link << "]\n";
        return false;
    }*/
    
    string readBuffer;

    struct curl_slist* headers = SetCURL(curl, &readBuffer, url, "", "", "POST");

    long httpCode = 0;
    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        cerr << "KV POST failed for [" << link << "]: " << curl_easy_strerror(res) << endl;
        return false;
    }

    return httpCode == 201;
}

bool PostHTMLContent(CURL* curl, const string link, const string Body) {
    string url = config.HTML_STORAGE_ENDPOINT + "/" + link;
    string readBuffer;
    struct curl_slist* headers = NULL;

    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, Body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, Body.length());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

    headers = curl_slist_append(headers, config.USER_AGENT.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    long httpCode = 0;
    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        cerr << "HTML POST failed for [" << link << "]: " << curl_easy_strerror(res) << endl;
        return false;
    }

    return httpCode == 200;
}
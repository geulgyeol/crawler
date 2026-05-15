#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <map>
#include <pulsar/Client.h>
#include <yaml-cpp/yaml.h>

using namespace std;

class Config {
public:
	Config();

	const YAML::Node config;

	const string CRAWLER_NAME;							// Crawler Name

	const string USER_AGENT;							// User Agent

	const string LINK_KV_ENDPOINT;						// link-kv
	const string HTML_STORAGE_ENDPOINT;					// html-storage
	const string PULSAR_SERVICE_URL;					// Pulsar 서비스 URL

	const string PULSAR_NAMESPACE;						// 

	const int MAX_CONCURRENT_REQUESTS;					// Max Concurrent Requests Count (Keep the default value in most cases: 10)
	
	const int BODIES_THRESHOLD;							// Threshold to Upload on html-storage

	const int NAVER_TIMEOUT_WAITING_TIME;				// Waiting Time When Naver Blog Timeouted

	const int CONNECTION_TIMEOUT_SECONDS;				// Connection Timeout Seconds
	const int RESPONSE_TIMEOUT_SECONDS;					// Response Timeout Seconds

	const int MAX_MESSAGE_QUEUE_SIZE;					// 메세지 큐 최대 크기

	const int MAX_BATCHING_MESSAGE_COUNT;				// 배치 최대 크기
	const int MAX_BATCHING_DELAY;						// 배치 최대 딜레이 (ms)

	const long long ROBOTS_CACHE_DURATION_SECONDS;		// robots.txt Cache Refresh Duration
	const size_t MAX_ROBOTS_CACHE_SIZE;					// robots.txt Cache Max Count Limit (If exceeded limit, clear all cathy)

	const bool VERBOSE;									// enable debug output
	
	const pulsar::Logger::Level LOG_LEVEL;				// Pulsar 로그 레벨 (INFO, DEBUG, WARN, ERROR)

	map<const string, const int> CRAWL_PER_SECOND_MAP;	// Crawl Per Second (**DONT** change string, 10 = crawl per 0.1s)
};

#endif

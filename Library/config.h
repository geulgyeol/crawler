#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <map>

using namespace std;

class Config {
public:
	Config();

	const string CRAWLER_NAME;							// Crawler Name

	const string USER_AGENT;							// User Agent

	const string LINK_KV_ENDPOINT;						// link-kv
	const string HTML_STORAGE_ENDPOINT;					// html-storage
	const string QUEUE_ENDPOINT;						// queue

	const int MAX_CONCURRENT_REQUESTS;					// Max Concurrent Requests Count (Keep the default value in most cases: 10)
	
	const int BODIES_THRESHOLD;							// Threshold to Upload on html-storage

	const int NAVER_TIMEOUT_WAITING_TIME;				// Waiting Time When Naver Blog Timeouted

	const int QUEUE_TIME_LIMIT;							// rnlcksgdk wntjr dksekfdk

	const long CONNECTION_TIMEOUT_SECONDS;				// Connection Timeout Seconds
	const long RESPONSE_TIMEOUT_SECONDS;				// Response Timeout Seconds

	const int ENABLE_MESSAGE_QUEUE_THRESHOLD;			// Threshold to Enable Subscriber
	const int DISABLE_MESSAGE_QUEUE_THRESHOLD;			// Threshold to Disable Subscriber

	const long long ROBOTS_CACHE_DURATION_SECONDS;		// robots.txt Cache Refresh Duration
	const size_t MAX_ROBOTS_CACHE_SIZE;					// robots.txt Cache Max Count Limit (If exceeded limit, clear all cathy)

	const bool VERBOSE;									// enable debug output

	map<const string, const int> CRAWL_PER_SECOND_MAP;	// Crawl Per Second (**DONT** change string, 10 = crawl per 0.1s)
};

#endif

#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <map>

using namespace std;

class Config {
public:
	Config();

	const string PROJECT_ID;							// GCP Project ID

	const string PROFILE_TOPIC_ID;						// Profile Topic ID
	const string WRITING_TOPIC_ID;						// Writing Topic ID

	const string PROFILE_SUB_ID;						// Profile Subscription ID (Profile Topic)
	const string WRITING_FOR_PROFILE_SUB_ID;			// Writing For Profile Subscription ID (Writing Topic)
	const string WRITING_FOR_CONTENT_SUB_ID;			// Writing For Content Subscription ID (Writing Topic)

	const string CRAWLER_NAME;							// Crawler Name

	const string USER_AGENT;							// User Agent

	const int MAX_CONCURRENT_REQUESTS;					// Max Concurrent Requests Count (**NOT RECOMMEND** change Default: 10)

	const long long ROBOTS_CACHE_DURATION_SECONDS;		// robots.txt Cache Refresh Duration
	const size_t MAX_ROBOTS_CACHE_SIZE;					// robots.txt Cache Max Count Limit (If exceeded limit, clear all cathy)

	map<const string, const int> CRAWL_PER_SECOND_MAP;	// Crawl Per Second (**DONT** change string, 10 = crawl per 0.1s)
};

#endif

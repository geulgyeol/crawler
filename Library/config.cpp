#include "pch.h"
#include "config.h"
#include <string>
using namespace std;

Config::Config() : 
    PROJECT_ID("geulgyeoltest"),
    PROFILE_TOPIC_ID("blogProfileTopic"),
    WRITING_TOPIC_ID("blogWritingLink"),
    PROFILE_SUB_ID("blogProfileSub"),
    WRITING_FOR_PROFILE_SUB_ID("blogWritingLinkForProfileSub"),
    WRITING_FOR_CONTENT_SUB_ID("blogWritingLinkForContentSub"),
    CRAWLER_NAME("geulgyeol-crawler"),
    USER_AGENT("User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:144.0; compatible; " + CRAWLER_NAME + "/2.0; +https://github.com/geulgyeol) Gecko/20100101 Firefox/144.0"),
    MAX_CONCURRENT_REQUESTS(10),
    ROBOTS_CACHE_DURATION_SECONDS(3600),
    MAX_ROBOTS_CACHE_SIZE(100)

{
    CRAWL_PER_SECOND_MAP.insert({ "LinkFinder_N", 4 });
    CRAWL_PER_SECOND_MAP.insert({ "LinkFinder_T", 20 });
    CRAWL_PER_SECOND_MAP.insert({ "ProfileFinder_N", 4 });
    CRAWL_PER_SECOND_MAP.insert({ "ProfileFinder_T", 4 });
    CRAWL_PER_SECOND_MAP.insert({ "HTMLCrawler_N", 10 });
    CRAWL_PER_SECOND_MAP.insert({ "HTMLCrawler_T", 20 });
}

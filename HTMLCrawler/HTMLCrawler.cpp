#include "../Library/ignore.h"
#include "../Library/Library.cpp"

using namespace std;
namespace pubsub = ::google::cloud::pubsub;

const std::string project_id = secrets.project_id;

const std::string blogProfileTopic_id = secrets.blogProfileTopic_id;
const std::string blogWritingLink_id = secrets.blogWritingLink_id;

const std::string blogProfileSub_id = secrets.blogProfileSub_id;
const std::string blogWritingLinkForProfileSub_id = secrets.blogWritingLinkForProfileSub_id;
const std::string blogWritingLinkForContentSub_id = secrets.blogWritingLinkForContentSub_id;

const int MAX_CONCURRENT_REQUESTS = 10;

const int CRAWL_PER_SECOND = 20;
const int DELAY_MILLI = 1000 / CRAWL_PER_SECOND;

std::unique_ptr<pubsub::Publisher> blogProfilePublisher;
std::unique_ptr<pubsub::Publisher> blogWritingPublisher;

std::unique_ptr<pubsub::Subscriber> blogProfileSubscriber;
std::unique_ptr<pubsub::Subscriber> blogWritingLinkForProfileSubscriber;
std::unique_ptr<pubsub::Subscriber> blogWritingLinkForContentSubscriber;

CURL* create_and_add_handle(CURLM* multi_handle, const std::string& link, std::map<CURL*, std::string*>& buffers, std::map<CURL*, std::string>& link_data) {
    if (link.empty()) return nullptr;

    CURL* eh = curl_easy_init();
    if (!eh) {
        std::cerr << "Failed to initialize CURL easy handle." << std::endl;
        return nullptr;
    }

    int slashIndex = link.find('/');
    std::string profileName = link.substr(1, slashIndex - 1);
    std::string writingNumber = link.substr(slashIndex + 1);
    std::string url;
    std::string* readBuffer = new std::string();

    if (link[0] == 'N') {
        url = "https://blog.naver.com/PostView.nhn?blogId=" + profileName + "&logNo=" + writingNumber;
    }
    else if (link[0] == 'T') {
        url = "https://" + profileName + ".tistory.com/m/" + writingNumber;
    }
    else {
        delete readBuffer;
        curl_easy_cleanup(eh);
        return nullptr;
    }

    if (!IsAllowedByRobotsGeneral(url)) {
        cout << "SKIP: Robots.txt denied access for [" << link << "] URL [" << url << "]\\n";
        Delay(DELAY_MILLI);
        return nullptr;
    }

    SetCURL(eh, readBuffer, url);

    curl_multi_add_handle(multi_handle, eh);
    buffers[eh] = readBuffer;
    link_data[eh] = link;

    return eh;
}


int main() {
    std::cin.tie(NULL);
    std::ios::sync_with_stdio(false);

    blogProfilePublisher = std::make_unique<pubsub::Publisher>(pubsub::Publisher(pubsub::MakePublisherConnection(pubsub::Topic(project_id, blogProfileTopic_id), google::cloud::Options{}.set<pubsub::MessageOrderingOption>(true))));
    blogWritingPublisher = std::make_unique<pubsub::Publisher>(pubsub::Publisher(pubsub::MakePublisherConnection(pubsub::Topic(project_id, blogWritingLink_id), google::cloud::Options{}.set<pubsub::MessageOrderingOption>(true))));

    blogProfileSubscriber = std::make_unique<pubsub::Subscriber>(pubsub::Subscriber(pubsub::MakeSubscriberConnection(pubsub::Subscription(project_id, blogProfileSub_id))));
    blogWritingLinkForProfileSubscriber = std::make_unique<pubsub::Subscriber>(pubsub::Subscriber(pubsub::MakeSubscriberConnection(pubsub::Subscription(project_id, blogWritingLinkForProfileSub_id))));
    blogWritingLinkForContentSubscriber = std::make_unique<pubsub::Subscriber>(pubsub::Subscriber(pubsub::MakeSubscriberConnection(pubsub::Subscription(project_id, blogWritingLinkForContentSub_id))));

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURLM* multi_handle = curl_multi_init();

    if (!multi_handle) {
        std::cerr << "Failed to initialize CURL multi handle." << std::endl;
        curl_global_cleanup();
        return 0;
    }

    std::map<CURL*, std::string*> buffers;
    std::map<CURL*, std::string> link_data;

    int links_index = 0;
    int cnt = 0;

    while (true) {
        std::vector<std::string> links = Subscribe(*blogWritingLinkForContentSubscriber, 100);

        if (links[0] == "/TIMEOUTED") {
            if (buffers.empty()) continue;
            links.clear();
            links_index = 0;
        }
        else {
            links_index = 0;
        }

        int running_handles = 0;

        while (links_index < links.size() || running_handles > 0) {

            while (running_handles < MAX_CONCURRENT_REQUESTS && links_index < links.size()) {
                CURL* eh = create_and_add_handle(multi_handle, links[links_index], buffers, link_data);
                if (eh) {
                    curl_multi_perform(multi_handle, &running_handles);
                }
                links_index++;
            }

            if (running_handles > 0) {
                int numfds = 0;
                CURLMcode mc = curl_multi_wait(multi_handle, NULL, 0, 1000, &numfds);
                if (mc != CURLM_OK) break;

                mc = curl_multi_perform(multi_handle, &running_handles);
                if (mc != CURLM_OK) break;
            }

            CURLMsg* msg;
            int msgs_left;
            while ((msg = curl_multi_info_read(multi_handle, &msgs_left))) {
                if (msg->msg == CURLMSG_DONE) {
                    CURL* eh = msg->easy_handle;

                    std::string* buffer = buffers[eh];
                    std::string link = link_data[eh];
                    long response_code;
                    curl_easy_getinfo(eh, CURLINFO_RESPONSE_CODE, &response_code);

                    if (msg->data.result == CURLE_OK && response_code < 400) {
                        cout << "success: " << ++cnt << "\n";
                    }
                    else {
                        std::cerr << "FAILED for [" << link << "] (Code: " << response_code << "). Error: " << curl_easy_strerror(msg->data.result) << std::endl;
                    }

                    string Body;

                    Body.append("{\"body\":\"" + regex_replace(*buffer, std::regex("\""), "\\\"") + "\",\"blog\":\"");
                    if (!link.empty() && link[0] == 'N') {
                        Body.append("naver");
                    }
                    else if (!link.empty() && link[0] == 'T') {
                        Body.append("tistory");
                    }
                    Body.append("\",\"timestamp\":" + to_string(chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()) + "}");

                    delete &Body;
                    delete buffer;
                    buffers.erase(eh);
                    link_data.erase(eh);
                    curl_multi_remove_handle(multi_handle, eh);
                    curl_easy_cleanup(eh);

                    Delay(DELAY_MILLI);
                }
            }

            if (links_index >= links.size() && running_handles == 0) {
                break;
            }
        }

        std::cout << "All links processed for this batch. Total remaining handles: " << buffers.size() << std::endl;
    }

    curl_multi_cleanup(multi_handle);
    curl_global_cleanup();

    return 0;
}
#ifndef SECRETS_H
#define SECRETS_H

#include <string>

using namespace std;

class Secrets {
public:
	Secrets();

	const string project_id;

	const string blogProfileTopic_id;
	const string blogWritingLink_id;

	const string blogProfileSub_id;
	const string blogWritingLinkForProfileSub_id;
	const string blogWritingLinkForContentSub_id;

	const string NID_SES;
	const string NID_AUT;
};

#endif

#include "../../includes/Channels.hpp"

channels::channels() : topic("No topic is set")
{
    this->invit_only = 0;
    this->topic_rest = 0;
    user_limit = -1;
    this->pswrd = "";
}

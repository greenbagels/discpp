

#include "dis.hpp"

int main()
{
    discpp::connection testclass{};
    //auto testclass = std::make_shared<discpp::connection>();
    testclass.get_gateway();
    testclass.gateway_connect();
    testclass.main_loop();
    return 0;
}

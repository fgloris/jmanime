#include <boost/lambda/lambda.hpp>
#include <iostream>
#include <algorithm>

int main()
{
    using namespace boost::lambda;
    typedef std::istream_iterator<int> in;

    std::cout << "Enter numbers: ";

    // Read a sequence of integers from standard input, use Boost.Lambda to multiply each number by three, then write it to the standard output
    std::for_each(
        in(std::cin), in(), std::cout << (_1 * 3) << " ");
}
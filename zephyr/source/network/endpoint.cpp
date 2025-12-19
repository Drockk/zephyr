#include "zephyr/network/endpoint.hpp"

#include "zephyr/network/details/protocolConcept.hpp"

namespace zephyr::network
{
template class Endpoint<details::TcpTag>;
template class Endpoint<details::UdpTag>;
}  // namespace zephyr::network

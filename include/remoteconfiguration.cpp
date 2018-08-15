/**
\file    remoteconfiguration.cpp
\author  Tobias Kussel <kussel@cbs.tu-darmstadt.de>
\copyright SEL - Secure EpiLinker
    Copyright (C) 2018 Computational Biology & Simulation Group TU-Darmstadt
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Affero General Public License for more details.
    You should have received a copy of the GNU Affero General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
\brief Holds Information about a remote connection
*/

#include "remoteconfiguration.h"
#include <mutex>
#include "apikeyconfig.hpp"
#include "logger.h"
#include "resttypes.h"
#include "restutils.h"
#include "seltypes.h"
#include "util.h"
#include <restbed>
#include "logger.h"

#include "connectionhandler.h"
#include "serverhandler.h"
using namespace std;
namespace sel {

RemoteConfiguration::RemoteConfiguration(RemoteId c_id)
    : m_connection_id(move(c_id)) {}

RemoteConfiguration::~RemoteConfiguration() {}

Port RemoteConfiguration::get_remote_signaling_port() const {
  restbed::Uri address{m_connection_profile.url};
  return address.get_port();
}

string RemoteConfiguration::get_remote_scheme() const {
  restbed::Uri address{m_connection_profile.url};
  return address.get_scheme();
}

string RemoteConfiguration::get_remote_host() const {
  restbed::Uri address{m_connection_profile.url};
  return address.get_authority();
}

void RemoteConfiguration::set_connection_profile(ConnectionConfig cconfig) {
  m_connection_profile = std::move(cconfig);
}
void RemoteConfiguration::set_linkage_service(ConnectionConfig cconfig) {
  m_linkage_service = std::move(cconfig);
}

RemoteId RemoteConfiguration::get_id() const {
  return m_connection_id;
}

Port RemoteConfiguration::get_aby_port() const {
  return m_aby_port;
}

void RemoteConfiguration::set_aby_port(Port port) {
  m_aby_port = port;
}

void RemoteConfiguration::set_matching_mode(bool matching_mode) {
  m_matching_mode = matching_mode;
}

bool RemoteConfiguration::get_matching_mode() const {
  return m_matching_mode;
}

void RemoteConfiguration::mark_mutually_initialized() const {
  m_mutually_initialized = true;
}

bool RemoteConfiguration::get_mutual_initialization_status() const {
  return m_mutually_initialized;
}

void RemoteConfiguration::test_configuration(
    const RemoteId& client_id,
    const nlohmann::json& client_config,
    const shared_ptr<ConnectionHandler>& connection_handler,
    const shared_ptr<ServerHandler>& server_handler) {
  auto logger{get_default_logger()};
  auto data = client_config.dump();
  const auto auth{dynamic_cast<const APIKeyConfig*>(
      m_connection_profile.authentication.get())};
  list<string> headers{"Authorization: "s + auth->get_key(),
                       "Content-Type: application/json" };
  string url{assemble_remote_url(this) + "/testConfig/" + client_id};

  logger->debug("Sending config test to: {}\n", url);
  auto response{perform_post_request(url, data, headers, true)};
  logger->trace("Config test response:\n{} - {}\n", response.return_code, response.body );

  if(response.body.find("No connection initialized") != response.body.npos){
    logger->info("Waiting for remote side to initialize connection");
    return;
  }
  if(response.body.find("Configurations are not compatible") != response.body.npos){
    logger->error("Configuration is not compatible to remote config");
    return;
  }
  const auto common_port{get_headers(response.body, "SEL-Port")};
  if (!common_port.empty()) {
    logger->info("Client registered common Port {}", common_port.front());
    set_aby_port(stoul(common_port.front()));
    mark_mutually_initialized();
    try {
      connection_handler->mark_port_used(m_aby_port);
    } catch (const exception& e) {
      logger->warn(
          "Can not mark port as used. If server and client are the same "
          "process, that is ok.");
    }
    auto fun = bind(&ServerHandler::insert_client, server_handler.get(),
                    placeholders::_1);
    std::thread client_creator(fun, m_connection_id);
    client_creator.detach();
  }
}

ConnectionConfig const * RemoteConfiguration::get_linkage_service() const {
return &m_linkage_service;
}

}  // namespace sel

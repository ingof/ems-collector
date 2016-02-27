/*
 * Buderus EMS data collector
 *
 * Copyright (C) 2011 Danny Baumann <dannybaumann@web.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __COMMANDHANDLER_H__
#define __COMMANDHANDLER_H__

#include <set>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/logic/tribool.hpp>
#include "EmsMessage.h"
#include "TcpHandler.h"

class CommandHandler;

class CommandConnection : public boost::enable_shared_from_this<CommandConnection>,
			  private boost::noncopyable
{
    public:
	typedef boost::shared_ptr<CommandConnection> Ptr;

    public:
	CommandConnection(CommandHandler& handler);

    public:
	boost::asio::ip::tcp::socket& socket() {
	    return m_socket;
	}
	void startRead() {
	    boost::asio::async_read_until(m_socket, m_request, "\n",
		boost::bind(&CommandConnection::handleRequest, shared_from_this(),
			    boost::asio::placeholders::error));
	}
	void close() {
	    m_socket.close();
	}
	void onIncomingMessage(const EmsMessage& message);
	void onTimeout();

    public:
	static std::string buildRecordResponse(const EmsProto::ErrorRecord *record);
	static std::string buildRecordResponse(const EmsProto::ScheduleEntry *entry);
	static std::string buildRecordResponse(const char *type, const EmsProto::HolidayEntry *entry);

    private:
	void handleRequest(const boost::system::error_code& error);
	void handleWrite(const boost::system::error_code& error);

	class CommandClient : public EmsCommandClient {
	    public:
		CommandClient(CommandConnection *connection) :
		    m_connection(connection)
		{}
		void onIncomingMessage(const EmsMessage& message) override {
		    m_connection->onIncomingMessage(message);
		}
		void onTimeout() {
		    m_connection->onTimeout();
		}

	    private:
		CommandConnection *m_connection;
	};

	typedef enum {
	    Ok,
	    InvalidCmd,
	    InvalidArgs
	} CommandResult;

	CommandResult handleCommand(std::istream& request);
	CommandResult handleRcCommand(std::istream& request);
	CommandResult handleUbaCommand(std::istream& request);
#if defined(HAVE_RAW_READWRITE_COMMAND)
	CommandResult handleRawCommand(std::istream& request);
#endif
	CommandResult handleCacheCommand(std::istream& request);
	CommandResult handleHkCommand(std::istream& request, uint8_t base);
	CommandResult handleSingleByteValue(std::istream& request, uint8_t dest, uint8_t type,
					    uint8_t offset, int multiplier, int min, int max);
	CommandResult handleSetHolidayCommand(std::istream& request, uint8_t type, uint8_t offset);
	CommandResult handleWwCommand(std::istream& request);
	CommandResult handleThermDesinfectCommand(std::istream& request);
	CommandResult handleZirkPumpCommand(std::istream& request);

	template<typename T> boost::tribool loopOverResponse(const char *prefix = "");

	bool parseScheduleEntry(std::istream& request, EmsProto::ScheduleEntry *entry);
	bool parseHolidayEntry(const std::string& string, EmsProto::HolidayEntry *entry);

	void respond(const std::string& response) {
	    boost::asio::async_write(m_socket, boost::asio::buffer(response + "\n"),
		boost::bind(&CommandConnection::handleWrite, shared_from_this(),
			    boost::asio::placeholders::error));
	}
	boost::tribool handleResponse();
	void startRequest(uint8_t dest, uint8_t type, size_t offset, size_t length,
			  bool newRequest = true, bool raw = false);
	bool continueRequest();
	void sendCommand(uint8_t dest, uint8_t type, uint8_t offset,
			 const uint8_t *data, size_t count,
			 bool expectResponse = false);
	void sendActiveRequest();
	bool parseIntParameter(std::istream& request, uint8_t& data, uint8_t max);

    private:
	static const unsigned int MaxRequestRetries = 5;

	boost::asio::ip::tcp::socket m_socket;
	boost::asio::streambuf m_request;
	std::shared_ptr<EmsCommandClient> m_commandClient;
	CommandHandler& m_handler;
	unsigned int m_responseCounter;
	unsigned int m_retriesLeft;
	std::shared_ptr<EmsMessage> m_activeRequest;
	std::vector<uint8_t> m_requestResponse;
	size_t m_requestOffset;
	size_t m_requestLength;
	uint8_t m_requestDestination;
	uint8_t m_requestType;
	size_t m_parsePosition;
	bool m_outputRawData;
};

class CommandHandler : private boost::noncopyable
{
    public:
	CommandHandler(TcpHandler& handler,
		       boost::asio::ip::tcp::endpoint& endpoint);
	~CommandHandler();

    public:
	void startConnection(CommandConnection::Ptr connection);
	void stopConnection(CommandConnection::Ptr connection);
	TcpHandler& getHandler() const {
	    return m_handler;
	}

    private:
	void handleAccept(CommandConnection::Ptr connection,
			  const boost::system::error_code& error);
	void startAccepting();

    private:
	TcpHandler& m_handler;
	boost::asio::ip::tcp::acceptor m_acceptor;
	std::set<CommandConnection::Ptr> m_connections;
};

#endif /* __COMMANDHANDLER_H__ */

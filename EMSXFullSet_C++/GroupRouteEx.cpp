/* Copyright 2017. Bloomberg Finance L.P.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to
* deal in the Software without restriction, including without limitation the
* rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
* sell copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:  The above
* copyright notice and this permission notice shall be included in all copies
* or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
* IN THE SOFTWARE.
*/
#include "BlpThreadUtil.h"

#include <blpapi_correlationid.h>
#include <blpapi_element.h>
#include <blpapi_event.h>
#include <blpapi_message.h>
#include <blpapi_name.h>
#include <blpapi_session.h>
#include <blpapi_subscriptionlist.h>

#include <cassert>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <time.h>
#include <vector>

using namespace BloombergLP;
using namespace blpapi;

namespace {

	Name SESSION_STARTED("SessionStarted");
	Name SESSION_STARTUP_FAILURE("SessionStartupFailure");
	Name SERVICE_OPENED("ServiceOpened");
	Name SERVICE_OPEN_FAILURE("ServiceOpenFailure");
	Name ERROR_INFO("ErrorInfo");
	Name GROUP_ROUTE_EX("GroupRouteEx");

	const std::string d_service("//blp/emapisvc_beta");
	CorrelationId requestID;

}

class ConsoleOut
{
private:
	std::ostringstream  d_buffer;
	Mutex              *d_consoleLock;
	std::ostream&       d_stream;

	// NOT IMPLEMENTED
	ConsoleOut(const ConsoleOut&);
	ConsoleOut& operator=(const ConsoleOut&);

public:
	explicit ConsoleOut(Mutex         *consoleLock,
		std::ostream&  stream = std::cout)
		: d_consoleLock(consoleLock)
		, d_stream(stream)
	{}

	~ConsoleOut() {
		MutexGuard guard(d_consoleLock);
		d_stream << d_buffer.str();
		d_stream.flush();
	}

	template <typename T>
	std::ostream& operator<<(const T& value) {
		return d_buffer << value;
	}

	std::ostream& stream() {
		return d_buffer;
	}
};

struct SessionContext
{
	Mutex            d_consoleLock;
	Mutex            d_mutex;
	bool             d_isStopped;
	SubscriptionList d_subscriptions;

	SessionContext()
		: d_isStopped(false)
	{
	}
};

class EMSXEventHandler : public EventHandler
{
	bool                     d_isSlow;
	SubscriptionList         d_pendingSubscriptions;
	std::set<CorrelationId>  d_pendingUnsubscribe;
	SessionContext          *d_context_p;
	Mutex                   *d_consoleLock_p;

	bool processSessionEvent(const Event &event, Session *session)
	{

		ConsoleOut(d_consoleLock_p) << "Processing SESSION_EVENT" << std::endl;

		MessageIterator msgIter(event);
		while (msgIter.next()) {

			Message msg = msgIter.message();

			if (msg.messageType() == SESSION_STARTED) {
				ConsoleOut(d_consoleLock_p) << "Session started..." << std::endl;
				session->openServiceAsync(d_service.c_str());
			}
			else if (msg.messageType() == SESSION_STARTUP_FAILURE) {
				ConsoleOut(d_consoleLock_p) << "Session startup failed" << std::endl;
				return false;
			}
		}
		return true;
	}

	bool processServiceEvent(const Event &event, Session *session)
	{

		ConsoleOut(d_consoleLock_p) << "Processing SERVICE_EVENT" << std::endl;

		MessageIterator msgIter(event);
		while (msgIter.next()) {

			Message msg = msgIter.message();

			if (msg.messageType() == SERVICE_OPENED) {
				ConsoleOut(d_consoleLock_p) << "Service opened..." << std::endl;

				Service service = session->getService(d_service.c_str());

				Request request = service.createRequest("GroupRouteEx");

				// Multiple order numbers can be added
				request.append("EMSX_SEQUENCE", 3734835);
				request.append("EMSX_SEQUENCE", 3734836);
				request.append("EMSX_SEQUENCE", 3734837);

				// The fields below are mandatory
				request.set("EMSX_AMOUNT_PERCENT", 100); // Note the amount here is %age of order amount
				request.set("EMSX_BROKER", "BMTB");

				// For GroupRoute, the below values need to be added, but are taken 
				// from the original order when the route is created.
				request.set("EMSX_HAND_INSTRUCTION", "ANY");
				request.set("EMSX_ORDER_TYPE", "MKT");
				request.set("EMSX_TICKER", "IBM US Equity");
				request.set("EMSX_TIF", "DAY");

				// The fields below are optional
				//request.set("EMSX_ACCOUNT","TestAccount");
				//request.set("EMSX_BOOKNAME","BookName");
				//request.set("EMSX_CFD_FLAG", "1");
				//request.set("EMSX_CLEARING_ACCOUNT", "ClrAccName");
				//request.set("EMSX_CLEARING_FIRM", "FirmName");
				//request.set("EMSX_EXEC_INSTRUCTIONS", "AnyInst");
				//request.set("EMSX_GET_WARNINGS", "0");
				//request.set("EMSX_GTD_DATE", "20170105");
				//request.set("EMSX_LIMIT_PRICE", 123.45);
				//request.set("EMSX_LOCATE_BROKER", "BMTB");
				//request.set("EMSX_LOCATE_ID", "SomeID");
				//request.set("EMSX_LOCATE_REQ", "Y");
				//request.set("EMSX_NOTES", "Some notes");
				//request.set("EMSX_ODD_LOT", "0");
				//request.set("EMSX_P_A", "P");
				//request.set("EMSX_RELEASE_TIME", 34341);
				//request.set("EMSX_REQUEST_SEQ", 1001);
				//request.set("EMSX_STOP_PRICE", 123.5);
				//request.set("EMSX_TRADER_UUID", 1234567);

				// Set the Request Type if this is for multi-leg orders
				// only valid for options
				/*
				Element requestType = request.getElement("EMSX_REQUEST_TYPE");
				requestType.setChoice("Multileg");
				Element multileg = requestType.getElement("Multileg");
				multileg.setElement("EMSX_AMOUNT",10);
				multileg.getElement("EMSX_ML_RATIO").appendValue(2);
				multileg.getElement("EMSX_ML_RATIO").appendValue(3);
				*/

				// Add the Route Ref ID values
				Element routeRefIDPairs = request.getElement("EMSX_ROUTE_REF_ID_PAIRS");
				Element route1 = routeRefIDPairs.appendElement();
				route1.setElement("EMSX_ROUTE_REF_ID", "MyRouteRef1");
				route1.setElement("EMSX_SEQUENCE", 3663920);

				Element route2 = routeRefIDPairs.appendElement();
				route2.setElement("EMSX_ROUTE_REF_ID", "MyRouteRef2");
				route2.setElement("EMSX_SEQUENCE", 3663921);

				Element route3 = routeRefIDPairs.appendElement();
				route3.setElement("EMSX_ROUTE_REF_ID", "MyRouteRef3");
				route3.setElement("EMSX_SEQUENCE", 3663922);

				// Below we establish the strategy details. Strategy details
				// are common across all orders in a GroupRoute operation.

				Element strategy = request.getElement("EMSX_STRATEGY_PARAMS");
				strategy.setElement("EMSX_STRATEGY_NAME", "VWAP");

				Element indicator = strategy.getElement("EMSX_STRATEGY_FIELD_INDICATORS");
				Element data = strategy.getElement("EMSX_STRATEGY_FIELDS");

				// Strategy parameters must be appended in the correct order. See the output 
				// of GetBrokerStrategyInfo request for the order. The indicator value is 0 for 
				// a field that carries a value, and 1 where the field should be ignored

				data.appendElement().setElement("EMSX_FIELD_DATA", "09:30:00"); // StartTime
				indicator.appendElement().setElement("EMSX_FIELD_INDICATOR", 0);

				data.appendElement().setElement("EMSX_FIELD_DATA", "10:30:00"); // EndTime
				indicator.appendElement().setElement("EMSX_FIELD_INDICATOR", 0);

				data.appendElement().setElement("EMSX_FIELD_DATA", ""); 		// Max%Volume
				indicator.appendElement().setElement("EMSX_FIELD_INDICATOR", 1);

				data.appendElement().setElement("EMSX_FIELD_DATA", ""); 		// %AMSession
				indicator.appendElement().setElement("EMSX_FIELD_INDICATOR", 1);

				data.appendElement().setElement("EMSX_FIELD_DATA", ""); 		// OPG
				indicator.appendElement().setElement("EMSX_FIELD_INDICATOR", 1);

				data.appendElement().setElement("EMSX_FIELD_DATA", ""); 		// MOC
				indicator.appendElement().setElement("EMSX_FIELD_INDICATOR", 1);

				data.appendElement().setElement("EMSX_FIELD_DATA", ""); 		// CompletePX
				indicator.appendElement().setElement("EMSX_FIELD_INDICATOR", 1);

				data.appendElement().setElement("EMSX_FIELD_DATA", ""); 		// TriggerPX
				indicator.appendElement().setElement("EMSX_FIELD_INDICATOR", 1);

				data.appendElement().setElement("EMSX_FIELD_DATA", ""); 		// DarkComplete
				indicator.appendElement().setElement("EMSX_FIELD_INDICATOR", 1);

				data.appendElement().setElement("EMSX_FIELD_DATA", ""); 		// DarkCompPX
				indicator.appendElement().setElement("EMSX_FIELD_INDICATOR", 1);

				data.appendElement().setElement("EMSX_FIELD_DATA", ""); 		// RefIndex
				indicator.appendElement().setElement("EMSX_FIELD_INDICATOR", 1);

				data.appendElement().setElement("EMSX_FIELD_DATA", ""); 		// Discretion
				indicator.appendElement().setElement("EMSX_FIELD_INDICATOR", 1);

				ConsoleOut(d_consoleLock_p) << "Request: " << request << std::endl;

				requestID = CorrelationId();

				session->sendRequest(request, requestID);

			}
			else if (msg.messageType() == SERVICE_OPEN_FAILURE) {
				ConsoleOut(d_consoleLock_p) << "Error: Service failed to open" << std::endl;
				return false;
			}

		}
		return true;
	}

	bool processResponseEvent(const Event &event, Session *session)
	{
		ConsoleOut(d_consoleLock_p) << "Processing RESPONSE_EVENT" << std::endl;

		MessageIterator msgIter(event);
		while (msgIter.next()) {

			Message msg = msgIter.message();

			ConsoleOut(d_consoleLock_p) << "MESSAGE: " << msg << std::endl;

			if (msg.messageType() == ERROR_INFO) {

				int errorCode = msg.getElementAsInt32("ERROR_CODE");
				std::string errorMessage = msg.getElementAsString("ERROR_MESSAGE");

				ConsoleOut(d_consoleLock_p) << "ERROR CODE: " << errorCode << "\tERROR MESSAGE: " << errorMessage << std::endl;
			}
			else if (msg.messageType() == GROUP_ROUTE_EX) {

				int numValues = 0;

				if (msg.hasElement("EMSX_SUCCESS_ROUTES")) {

					Element success = msg.getElement("EMSX_SUCCESS_ROUTES");

					numValues = success.numValues();

					for (int i = 0; i < numValues; i++) {

						Element e = success.getValueAsElement(i);

						int emsx_sequence = e.getElementAsInt32("EMSX_SEQUENCE");
						int emsx_route_id = e.getElementAsInt32("EMSX_ROUTE_ID");

						ConsoleOut(d_consoleLock_p) << "Success: " << emsx_sequence << ", " << emsx_route_id << std::endl;
					}

					if (msg.hasElement("EMSX_FAILED_ROUTES")) {

						Element failed = msg.getElement("EMSX_FAILED_ROUTES");

						numValues = failed.numValues();

						for (int i = 0; i < numValues; i++) {

							Element e = failed.getValueAsElement(i);

							int emsx_sequence = e.getElementAsInt32("EMSX_SEQUENCE");
							int error_code = e.getElementAsInt32("ERROR_CODE");
							std::string error_message = e.getElementAsString("ERROR_MESSAGE");

							ConsoleOut(d_consoleLock_p) << "Failed: " << emsx_sequence << ", " << error_code << ": " << error_message << std::endl;
						}

					}
				}

				std::string message = msg.getElementAsString("MESSAGE");
				ConsoleOut(d_consoleLock_p) << "MESSAGE:" << message << std::endl;
			}

		}
		return true;
	}

	bool processMiscEvents(const Event &event)
	{
		ConsoleOut(d_consoleLock_p) << "Processing UNHANDLED event" << std::endl;

		MessageIterator msgIter(event);
		while (msgIter.next()) {
			Message msg = msgIter.message();
			ConsoleOut(d_consoleLock_p) << msg.messageType().string() << "\n" << msg << std::endl;
		}
		return true;
	}

public:
	EMSXEventHandler(SessionContext *context)
		: d_isSlow(false)
		, d_context_p(context)
		, d_consoleLock_p(&context->d_consoleLock)
	{
	}

	bool processEvent(const Event &event, Session *session)
	{
		try {
			switch (event.eventType()) {
			case Event::SESSION_STATUS: {
				MutexGuard guard(&d_context_p->d_mutex);
				return processSessionEvent(event, session);
			} break;
			case Event::SERVICE_STATUS: {
				MutexGuard guard(&d_context_p->d_mutex);
				return processServiceEvent(event, session);
			} break;
			case Event::RESPONSE: {
				MutexGuard guard(&d_context_p->d_mutex);
				return processResponseEvent(event, session);
			} break;
			default: {
				return processMiscEvents(event);
			}  break;
			}
		}
		catch (Exception &e) {
			ConsoleOut(d_consoleLock_p)
				<< "Library Exception !!!"
				<< e.description() << std::endl;
		}
		return false;
	}
};

class GroupRouteEx
{

	SessionOptions            d_sessionOptions;
	Session                  *d_session;
	EMSXEventHandler		 *d_eventHandler;
	SessionContext            d_context;

	bool createSession() {
		ConsoleOut(&d_context.d_consoleLock)
			<< "Connecting to " << d_sessionOptions.serverHost()
			<< ":" << d_sessionOptions.serverPort() << std::endl;

		d_eventHandler = new EMSXEventHandler(&d_context);
		d_session = new Session(d_sessionOptions, d_eventHandler);

		d_session->startAsync();

		return true;
	}

public:

	GroupRouteEx()
		: d_session(0)
		, d_eventHandler(0)
	{


		d_sessionOptions.setServerHost("localhost");
		d_sessionOptions.setServerPort(8194);
		d_sessionOptions.setMaxEventQueueSize(10000);
	}

	~GroupRouteEx()
	{
		if (d_session) delete d_session;
		if (d_eventHandler) delete d_eventHandler;
	}

	void run(int argc, char **argv)
	{
		if (!createSession()) return;

		// wait for enter key to exit application
		ConsoleOut(&d_context.d_consoleLock)
			<< "\nPress ENTER to quit" << std::endl;
		char dummy[2];
		std::cin.getline(dummy, 2);
		{
			MutexGuard guard(&d_context.d_mutex);
			d_context.d_isStopped = true;
		}
		d_session->stop();
		ConsoleOut(&d_context.d_consoleLock) << "\nExiting..." << std::endl;
	}
};

int main(int argc, char **argv)
{
	std::cout << "Bloomberg - EMSX API Example - GroupRouteEx" << std::endl;
	GroupRouteEx groupRouteEx;
	try {
		groupRouteEx.run(argc, argv);
	}
	catch (Exception &e) {
		std::cout << "Library Exception!!!" << e.description() << std::endl;
	}
	// wait for enter key to exit application
	std::cout << "Press ENTER to quit" << std::endl;
	char dummy[2];
	std::cin.getline(dummy, 2);
	return 0;
}

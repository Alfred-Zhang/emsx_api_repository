﻿/* Copyright 2017. Bloomberg Finance L.P.
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

using Name = Bloomberglp.Blpapi.Name;
using SessionOptions = Bloomberglp.Blpapi.SessionOptions;
using Session = Bloomberglp.Blpapi.Session;
using Service = Bloomberglp.Blpapi.Service;
using Request = Bloomberglp.Blpapi.Request;
using Element = Bloomberglp.Blpapi.Element;
using CorrelationID = Bloomberglp.Blpapi.CorrelationID;
using Event = Bloomberglp.Blpapi.Event;
using Message = Bloomberglp.Blpapi.Message;
using EventHandler = Bloomberglp.Blpapi.EventHandler;
using System;

namespace com.bloomberg.emsx.samples
{
    public class AssignTrader
    {

        private static readonly Name SESSION_STARTED = new Name("SessionStarted");
        private static readonly Name SESSION_STARTUP_FAILURE = new Name("SessionStartupFailure");
        private static readonly Name SERVICE_OPENED = new Name("ServiceOpened");
        private static readonly Name SERVICE_OPEN_FAILURE = new Name("ServiceOpenFailure");

        private static readonly Name ERROR_INFO = new Name("ErrorInfo");
        private static readonly Name ASSIGN_TRADER = new Name("AssignTrader");

        private string d_service;
        private string d_host;
        private int d_port;

        private static bool quit = false;

        private CorrelationID requestID;

        public static void Main(String[] args)
        {
            System.Console.WriteLine("Bloomberg - EMSX API Example - AssignTrader\n");

            AssignTrader example = new AssignTrader();
            example.run(args);

            while (!quit) { };

        }

        public AssignTrader()
        {

            // Define the service required, in this case the beta service, 
            // and the values to be used by the SessionOptions object
            // to identify IP/port of the back-end process.

            d_service = "//blp/emapisvc_beta";
            d_host = "localhost";
            d_port = 8194;

        }

        private void run(String[] args)
        {

            SessionOptions d_sessionOptions = new SessionOptions();
            d_sessionOptions.ServerHost = d_host;
            d_sessionOptions.ServerPort = d_port;

            Session session = new Session(d_sessionOptions, new EventHandler(processEvent));

            session.StartAsync();

        }

        public void processEvent(Event evt, Session session)
        {
            try
            {
                switch (evt.Type)
                {
                    case Event.EventType.SESSION_STATUS:
                        processSessionEvent(evt, session);
                        break;
                    case Event.EventType.SERVICE_STATUS:
                        processServiceEvent(evt, session);
                        break;
                    case Event.EventType.RESPONSE:
                        processResponseEvent(evt, session);
                        break;
                    default:
                        processMiscEvents(evt, session);
                        break;
                }
            }
            catch (Exception e)
            {
                System.Console.Error.WriteLine(e);
            }
        }

        private void processSessionEvent(Event evt, Session session)
        {
            System.Console.WriteLine("\nProcessing " + evt.Type);

            foreach (Message msg in evt)
            {
                if (msg.MessageType.Equals(SESSION_STARTED))
                {
                    System.Console.WriteLine("Session started...");
                    session.OpenServiceAsync(d_service);
                }
                else if (msg.MessageType.Equals(SESSION_STARTUP_FAILURE))
                {
                    System.Console.Error.WriteLine("Error: Session startup failed");
                }
            }
        }

        private void processServiceEvent(Event evt, Session session)
        {

            System.Console.WriteLine("\nProcessing " + evt.Type);

            foreach (Message msg in evt)
            {
                if (msg.MessageType.Equals(SERVICE_OPENED))
                {
                    System.Console.WriteLine("Service opened...");

                    Service service = session.GetService(d_service);

                    Request request = service.CreateRequest("AssignTrader");

                    //request.Set("EMSX_REQUEST_SEQ", 1);

                    request.Append("EMSX_SEQUENCE", 3657359);
                    request.Append("EMSX_SEQUENCE", 3657360);

                    request.Set("EMSX_ASSIGNEE_TRADER_UUID", 12109783);

                    System.Console.WriteLine("Request: " + request.ToString());

                    requestID = new CorrelationID();

                    // Submit the request
                    try
                    {
                        session.SendRequest(request, requestID);
                    }
                    catch (Exception ex)
                    {
                        System.Console.Error.WriteLine("Failed to send the request: " + ex.Message);
                    }

                }
                else if (msg.MessageType.Equals(SERVICE_OPEN_FAILURE))
                {
                    System.Console.Error.WriteLine("Error: Service failed to open");
                }
            }
        }

        private void processResponseEvent(Event evt, Session session)
        {
            System.Console.WriteLine("Received Event: " + evt.Type);

            foreach (Message msg in evt)
            {

                System.Console.WriteLine("MESSAGE: " + msg.ToString());
                System.Console.WriteLine("CORRELATION ID: " + msg.CorrelationID);

                if (evt.Type == Event.EventType.RESPONSE && msg.CorrelationID == requestID)
                {

                    System.Console.WriteLine("Message Type: " + msg.MessageType);
                    if (msg.MessageType.Equals(ERROR_INFO))
                    {
                        int errorCode = msg.GetElementAsInt32("ERROR_CODE");
                        String errorMessage = msg.GetElementAsString("ERROR_MESSAGE");
                        System.Console.WriteLine("ERROR CODE: " + errorCode + "\tERROR MESSAGE: " + errorMessage);
                    }
                    else if (msg.MessageType.Equals(ASSIGN_TRADER))
                    {
                        bool success = msg.GetElementAsBool("EMSX_ALL_SUCCESS");

                        if (success)
                        {
                            System.Console.WriteLine("All orders successfully assigned");
                            Element successful = msg.GetElement("EMSX_ASSIGN_TRADER_SUCCESSFUL_ORDERS");

                            int numValues = successful.NumValues;

                            if (numValues > 0) System.Console.WriteLine("Successful assignments:-");

                            for (int i = 0; i < numValues; i++)
                            {

                                Element order = successful.GetValueAsElement(i);
                                System.Console.WriteLine(order.GetElement("EMSX_SEQUENCE").GetValueAsInt32());
                            }
                        }
                        else
                        {

                            System.Console.WriteLine("One or more failed assignments...\n");

                            if (msg.HasElement("EMSX_ASSIGN_TRADER_SUCCESSFUL_ORDERS"))
                            {
                                Element successful = msg.GetElement("EMSX_ASSIGN_TRADER_SUCCESSFUL_ORDERS");

                                int numValues = successful.NumValues;

                                if (numValues > 0) System.Console.WriteLine("Successful assignments:-");

                                for (int i = 0; i < numValues; i++)
                                {

                                    Element order = successful.GetValueAsElement(i);
                                    System.Console.WriteLine(order.GetElement("EMSX_SEQUENCE").GetValueAsInt32());
                                }
                            }
                            if (msg.HasElement("EMSX_ASSIGN_TRADER_FAILED_ORDERS"))
                            {
                                Element failed = msg.GetElement("EMSX_ASSIGN_TRADER_FAILED_ORDERS");

                                int numValues = failed.NumValues;

                                if (numValues > 0) System.Console.WriteLine("Failed assignments:-");

                                for (int i = 0; i < numValues; i++)
                                {

                                    Element order = failed.GetValueAsElement(i);
                                    System.Console.WriteLine(order.GetElement("EMSX_SEQUENCE").GetValueAsInt32());
                                }
                            }
                        }
                    }

                    quit = true;
                    session.Stop();
                }
            }
        }

        private void processMiscEvents(Event evt, Session session)
        {
            System.Console.WriteLine("Processing " + evt.Type);

            foreach (Message msg in evt)
            {
                System.Console.WriteLine("MESSAGE: " + msg);
            }
        }
    }
}
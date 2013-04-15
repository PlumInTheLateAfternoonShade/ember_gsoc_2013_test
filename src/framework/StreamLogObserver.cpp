/*
 Copyright (C) 2002  Lakin Wecker
	
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details. 

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "StreamLogObserver.h"
#include <boost/date_time.hpp>
#include <thread>

namespace Ember {

    /**
     * Creates a new StreamLogObserver using default values.
     */
    StreamLogObserver::StreamLogObserver(std::ostream &out) 
        : myOut(out), mDetailed(false), mStart(boost::posix_time::microsec_clock::local_time())
    {
    }

    /**
     * Copy constructor.
     */
    StreamLogObserver::StreamLogObserver( const StreamLogObserver &source )
        : myOut(source.myOut), mDetailed(source.mDetailed), mStart(boost::posix_time::microsec_clock::local_time())
        {
        }

    //----------------------------------------------------------------------
    // Destructor

    /**
     * Deletes a StreamLogObserver instance.
     */
    StreamLogObserver::~StreamLogObserver () {
    	myOut.flush();
    }

    //----------------------------------------------------------------------
    // Implemented methods from Log::Observer

    /**
     * Prints out the message provided with file, line and datestamp to myOut;
     */
    void StreamLogObserver::onNewMessage(const std::string & message, const std::string & file, const int & line, 
                                                 const Log::MessageImportance & importance)
    {
    	boost::posix_time::ptime currentTime = boost::posix_time::microsec_clock::local_time();

        myOut.fill('0');
        myOut << "[";
        myOut.width(2);
        myOut << currentTime.time_of_day().hours() << ":";
        myOut.width(2);
        myOut << currentTime.time_of_day().minutes() << ":";
        myOut.width(2);			
        myOut << currentTime.time_of_day().seconds();
        if (mDetailed) {
        	myOut << "(";
			myOut.width(8);
			myOut << ((currentTime - mStart).total_microseconds()) << ":"<<  std::this_thread::get_id() <<")";
        }
        myOut << "] ";

        if(importance == Log::CRITICAL)
		{
			myOut << "CRITICAL";
		}
        else  if(importance == Log::FAILURE)
		{
			myOut << "FAILURE";
		} 
        else if(importance == Log::WARNING)
		{
			myOut << "WARNING";
		}
        else if(importance == Log::INFO)
		{
			myOut << "INFO";
		}
        else
		{
			myOut << "VERBOSE";
		}
        
        myOut << " " << message;
        
        #ifdef EMBER_LOG_SHOW_ORIGIN
            if(line != -1){
                myOut << " [" << file << "(" <<  line << ")]";
            }
        #endif

        myOut << std::endl;

    }

    void StreamLogObserver::setDetailed(bool enabled)
    {
    	mDetailed = enabled;
    }

}; //end namespace Ember

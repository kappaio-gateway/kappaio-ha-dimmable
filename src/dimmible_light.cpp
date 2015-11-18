//====================================================================================
//	The MIT License (MIT)
//
//	Copyright (c) 2011 Kapparock LLC
//
//	Permission is hereby granted, free of charge, to any person obtaining a copy
//	of this software and associated documentation files (the "Software"), to deal
//	in the Software without restriction, including without limitation the rights
//	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//	copies of the Software, and to permit persons to whom the Software is
//	furnished to do so, subject to the following conditions:
//
//	The above copyright notice and this permission notice shall be included in
//	all copies or substantial portions of the Software.
//
//	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//	THE SOFTWARE.
//====================================================================================

#include <vector>
#include <iostream>
#include <string>
#include <utility>
#include <initializer_list>
// headers from kappaBox SDK
#include "notification.h"
#include "zdo.h"
#include "apsdb.h"
#include "kjson.h"
#include "hal.h"
#include "restful.h"
// headers from this app
#include "dimmible_light.h"
#include "zcl.h"

namespace
{
namespace DimmibleLightNS
{
	// Constants from ZigBee Cluster Library
	const uint16_t HA_ProfileId = 0x0104;        // ZigBee Home Automation profile Id
	const uint16_t DimmableLightDevice = 0x0101; // Device ID for dimmable light
	const uint16_t OnOffClusterId = 0x0006;      // On/Off cluster
	const uint16_t LevelClusterId = 0x0008;      // Level controll cluster
	const uint16_t IdentifyClusterId = 0x0003;   // Identify
	const uint8_t DeviceVersion = 0x02;          // Device version

	using namespace aps;
	using namespace zcl;
	using namespace std;
	using namespace	kapi::notify;
	using JSON	= kapi::JSON;
	using Context = ApplicationInterface::Context;

	inline void get_attribute(aps::Cluster& cluster)
	{

		aps::APDU{ cluster , getAttr{ cluster } }.send( setAttr{ cluster } );
	}

	void init()
	{
		// Create a ZigBee endpoint with endpoint address 0x01 object in rsserial.
		// If endpoint address 0x01 is already taken, rsserial will return an
		// endpoint object with the next available endpoint address.
		aps::Endpoint& localEndpoint = thisDevice().newEndpoint( 1 );

		// Register the endpoint address, Profile ID , Device Id and Device Version with ZigBee hardware.
		// This is required in order for ZigBee hardware to route messages destinated to this endpoint
		registerEndpoint( localEndpoint.id() , HA_ProfileId , DimmableLightDevice , DeviceVersion );

		// Register the simple description with rsserial.
		// At the very least, profileId and deviceId are required for rsserial to "pair" endpoints when new device joins to the network.
		// The condition for pairing is defined as :
		// a) Both have the same profileId
		//           AND
		// b) Both have the same deviceId
		//
		// If pairing condition is satisfied, it will call the callback function registered with:
		//
		// handler("NewJoin", localEndpoint.uri(), Callback);
		//
		aps::SimpleDescriptor& simpleDescriptor = localEndpoint.simpleDescriptor();
		simpleDescriptor.profileId = HA_ProfileId;
		simpleDescriptor.deviceId = DimmableLightDevice;
		simpleDescriptor.deviceVersion = DeviceVersion;
		simpleDescriptor.outclusterList	= { 0x0006 , 0x0008 };

		// This file contains clusters definitions as listed for ZigBee HA dimmable light.
		// As the extension suggests is in json format. Clusters in this file is defined in terms
		// of attribute names, data types, ranges according to ZigBee Cluster Library.
		// This file is parsed by rsserial, and it will affect the output of localEndpoint.toJSON().
		//
		localEndpoint.setClusterLookupPath( "/usr/lib/rsserial/philips_hue/hueclusters.json" );

		// REST API definition
		// URI : <rootURL>/devices/0000/endpoints/<localEndpoint.id()>
		// Method: GET or POST
		// Response: current state of localEndpoint
		// Note : ApplicationInterface::EventTag =  "Application"
		handler(ApplicationInterface::EventTag, localEndpoint.uri(),
			[&localEndpoint](Context ctx)
			{
				ctx.response( localEndpoint.toJSON().stringify() );
			}
		);

		// Register callback to handle newly joined ZigBee endpoint
		// The chain of processes that cause this callback to execute:
		// 1) rsserial receive device_annce message from newly join or rejoin device
		// 2) rsserial collect device information from the device
		// 3) if any of endpoints of the devices have the same ProfileId and DeviceId, call this callback
		handler("NewJoin", localEndpoint.uri(),
			[](Endpoint& remoteEndpoint)
			{
				// REST API definition
				// URI : <rootURL>/devices/<remoteDevice's network address>/endpoints/<remoteEndpoint.id()>/states
				// Method: GET or POST
				// Response:
				//   1) Update the on/off state of the remote endoint
				//   2) Update the level value of remote endpoint
				//   3) return the current state of remoteEndpoint
				handler(ApplicationInterface::EventTag, remoteEndpoint.uri() + "/states",
					[&remoteEndpoint](Context ctx)
					{
						get_attribute( remoteEndpoint.clusters( OnOffClusterId ) );
						get_attribute( remoteEndpoint.clusters( LevelClusterId ) );
						ctx.response( remoteEndpoint.toJSON().stringify() );
					}
				);

				// REST API definition
				// URI : <rootURL>/devices/<remoteDevice's network address>/endpoints/<remoteEndpoint.id()>/toggle
				// Method: GET or POST
				// Response:
				//   1) If the the bulb is on , turn it off. If it is off, turn it on.
				//   2) Update the onoff value of remote endpoint
				//   3) return the current state
				handler(ApplicationInterface::EventTag, remoteEndpoint.uri() + "/toggle",
					[&remoteEndpoint](Context Ctx)
					{
						Cluster& cluster = remoteEndpoint.clusters( OnOffClusterId );
						APDU{ cluster , onoff{0x02}	}.send(
							[]( AFMessage& response )
							{
								// nothting to do
							}
						);

						APDU{ cluster , getAttr{ cluster } }.send( setAttr{ cluster } );
						APDU{ remoteEndpoint.clusters( LevelClusterId ) , getAttr{ remoteEndpoint.clusters( LevelClusterId ) } }.send( setAttr{ remoteEndpoint.clusters( LevelClusterId ) } );
						Ctx.response( remoteEndpoint.toJSON().stringify() );
					}
				);

				// REST API definition
				// URI : <rootURL>/devices/<remoteDevice's network address>/endpoints/<remoteEndpoint.id()>/level
				// Method: GET or POST
				// Argument:
				//       val : desired level. Between 0 to 65536. Represented by 4 hex digits
				//       rate : rate of transition for val in unit of 100ms. Between 0 to 65536. Represented by 4 hex digits
				// Response:
				//   1) Change the brightness to 'val' in 'rate'x100 ms
				//   2) Update the level value of remote endpoint
				//   3) return the current state
				handler(ApplicationInterface::EventTag, remoteEndpoint.uri() + "/level",
					[&remoteEndpoint](Context ctx)
					{
						JSON arg(ctx.parameter().c_str());
						uint8_t	val	= arg[ "val" ].toInteger();
						uint16_t rate =  arg[ "rate" ].toInteger();
						Cluster& cluster = remoteEndpoint.clusters( LevelClusterId );
						APDU{ cluster , level{ val , rate} }.send(
							[](AFMessage& response)
							{
								// nothing to do
							}
						);

						APDU{ cluster , getAttr{ cluster } }.send( setAttr{ cluster } );
						APDU{ remoteEndpoint.clusters( OnOffClusterId ) , getAttr{ remoteEndpoint.clusters( OnOffClusterId ) } }.send( setAttr{ remoteEndpoint.clusters( OnOffClusterId ) } );
						ctx.response( remoteEndpoint.toJSON().stringify() );
					}
				);

				// REST API definition
				// URI : <rootURL>/devices/<remoteDevice's network address>/endpoints/<remoteEndpoint.id()>/identify
				// Method: GET or POST
				// Argument:
				//       identifytime : number of seconds for which the bulb should in identify mode (usually flickering)
				// Response:
				//   1) send a identify command to the bulb
				//   2) return the current state of the bulb
				handler(ApplicationInterface::EventTag, remoteEndpoint.uri() + "/identify",
					[&remoteEndpoint](Context ctx)
					{
						JSON arg(ctx.parameter().c_str());
						uint16_t t = arg[ "identifytime" ].toInteger();
						Cluster& cluster = remoteEndpoint.clusters( IdentifyClusterId );
						APDU{ cluster , identify{ 0, t } }.send(
							[](AFMessage& repsonse)
							{
								// nothing to do
							}
						);
						ctx.response( remoteEndpoint.toJSON().stringify() );
					}
				);
			}
		);

		return;
	}
}
}

void init()
{
	DimmibleLightNS::init();
}

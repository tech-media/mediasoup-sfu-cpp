#include "Server.hpp"
// #include <webservice/file_request_handler.hpp>
// #include <webservice/ws_service.hpp>
// #include <webservice/ws_session.hpp>
// #include <webservice/server.hpp>
#include "Log.hpp"
#include <boost/lexical_cast.hpp>
#include "Transport/WebSocketServer.h"
#include "Transport/WebSocketTransport.h"
#include <iostream>
#include <csignal>
#include "RtpParameters.hpp"
#include "sdp/SdpOffer.hpp"

// struct mirror_ws_service: webservice::ws_service{
//     void on_open(webservice::ws_identifier identifier)override{
//         std::cout << "open session " << identifier << "\n";
//     }

//     void on_close(webservice::ws_identifier identifier)override{
//         std::cout << identifier << " closed\n";
//     }

//     void on_text(
//         webservice::ws_identifier identifier,
//         std::string&& text
//     )override{
//         std::cout << identifier << " received text message: " << text << "\n";

//         // Send received text to all WebSocket sessions
//         send_text(text);
//     }

//     void on_binary(
//         webservice::ws_identifier identifier,
//         std::vector< std::uint8_t >&& data
//     )override{
//         std::cout << identifier << " received binary message\n";

//         // Send received data to all WebSocket sessions
//         send_binary(data);
//     }
// };


// void on_interrupt(int signum){
//     std::signal(signum, SIG_DFL);
//     std::cout << "Signal: " << signum << "\n";
//    // server->shutdown();
//     std::cout << "Signal ready\n";
// }


// void print_help(char const* exec_name){
//     std::cerr << "Usage: " << exec_name
//         << " <address> <port> <doc_root> <thread_count>\n"
//         << "Example:\n"
//         << "    " << exec_name << " 0.0.0.0 8080 http_root_directory 1\n";
// }

Server::Server()
{
    
}
Server::~Server()
{
  // if(rawWebsockServer != nullptr) 
  // {
  //     delete rawWebsockServer;
  //     rawWebsockServer = nullptr;
  // }
  

  if(mediasoup != nullptr) {
      mediasoup->Destroy();
      mediasoup::DestroyMediasoup(mediasoup);
  }

  if(protooWebsockServer != nullptr) {
    //delete protooWebsockServer;
    protooWebsockServer = nullptr;
  }

}
int Server::init()
{
    initMediasoup();
    createRawWebSocket();
    runRawWebsocket();
   createProtooWebSocket();
    


}
void Server::run()
{
    runProtooWebSocketServer();

    //testProtoo();
}
int Server::createRawWebSocket()
{
//    try{
//        auto address = boost::asio::ip::make_address(this->listenip);
//        auto port = this->listenport;
//        rawWebsockServer = new webservice::server(
//                       nullptr,
//                       std::make_unique< mirror_ws_service >(),
//                       nullptr, // ignore errors and exceptions
//                       address, port, this->thread_count);
//        return 0;
//    }catch(std::exception const& e){
//        std::cerr << "Exception: " << e.what() << "\n";
//        return 1;
//    }catch(...){
//        std::cerr << "Unknown exception\n";
//        return 1;
//    }
    return 0;
}
int Server::createProtooWebSocket()
{
    protooWebsockServer = std::make_shared<WebSocketServer>("0.0.0.0",8001,"");//new WebSocketServer("0.0.0.0",8001,"");
    //protooWebsockServer->runWebSocketServer();
   
}
void Server::processRawSdpMessage(std::string message)
{
    auto msg = json::parse(message); //根据请求过来的数据来更新。
    MS_lOGD("raw websocket recv message %s", msg.dump().c_str());
    auto recipientClientId = msg["RecipientClientId"];
    auto messagePayload = msg["MessagePayload"];
    auto sdpAnswer = getStringFromBase64(messagePayload);
    MS_lOGD("messagePayload=%s",sdpAnswer.dump().c_str());
    auto roomId = msg["CorrelationId"];
    auto room = rooms[roomId];
    std::string bridgeId = msg["RecipientClientId"];
    auto transportId = room->getBridgeTransportId(bridgeId);
  
    

    try
    {
        auto dtlsParameters = mediasoupclient::Sdp::Offer::getMediasoupDtlsParameters(sdpAnswer["sdp"]);
        json jdtlsParameters = dtlsParameters;
        MS_lOGD("dtlsParameters=%s",jdtlsParameters.dump().c_str());
        room->connectBridgeTransport(
            
                bridgeId,
                transportId,
                dtlsParameters
        );



        std::string kind = "audio";
        
        auto rtpParameters = mediasoupclient::Sdp::Offer::getMediasoupRtpParameters(sdpAnswer["sdp"],kind,room->getLocalSdp());
        room->createBridgeProducer(
            
                bridgeId,
                transportId,
                kind,
                rtpParameters
            );

         kind = "video";
         rtpParameters = mediasoupclient::Sdp::Offer::getMediasoupRtpParameters(sdpAnswer["sdp"],kind,room->getLocalSdp());
        room->createBridgeProducer(

                bridgeId,
                transportId,
                kind,
                rtpParameters
            );
    }
    catch (...)
    {
        MS_lOGE("error");
    }
    if(msg["action"] == "SDP_OFFER") {
        auto webRtcTransport = room->getBridgeTransport(bridgeId);
        auto producers = room->getProducersFromBridge(bridgeId);
        auto producerMedias=mediasoupclient::Sdp::Offer::getMediasoupProducerMedias(sdpAnswer["sdp"]);
        auto sdpAnswer = mediasoupclient::Sdp::Offer::createWebrtcSdpAnswer(webRtcTransport,producers,producerMedias);
        json payload = {
            {"type","offer"},
            {"sdp",sdpAnswer}
        };
        Base64 base;
        auto msgpayload = base.Encode((const unsigned char*)payload.dump().c_str(),payload.dump().length());
        //auto b = new Buffer.from(JSON.stringify(payload));
       // auto msgpayload = b.toString("base64");
        json jsonmsg = {
            {"action","SDP_OFFER"},
            {"RecipientClientId","peerId"},
            {"MessagePayload",msgpayload}
        };
        //ws.send(JSON.stringify(jsonmsg));
    }
}
void Server::runRawWebsocket() 
{

}
void Server::runRawWebsockServer()
{
    //std::signal(SIGINT, &on_interrupt);
    //rawWebsockServer->block();
}
/**
 * Get next mediasoup Worker.
 */
std::shared_ptr<mediasoup::IWorker> Server::getMediasoupWorker()
{
	auto worker = workers[nextMediasoupWorkerIdx];

	if (++nextMediasoupWorkerIdx == workers.size())
		nextMediasoupWorkerIdx = 0;

	return worker;
}
void Server::processHttpRequest(std::string &path,std::string & roomId,std::shared_ptr<Room> room,json &params,json &query,json & body,json &respdata)
{
  	/**
	 * For every API request, verify that the roomId in the path matches and
	 * existing room->
	 */
/*  
	expressApp.param(
		"roomId", async (req, res, next, roomId) =>
		{
			//just create the room with roomId
			getOrCreateRoom({roomId});
			// The room must exist for all API requests.
			if (!rooms.has(roomId))
			{
				auto error = new Error("room with id "${roomId}" not found");

				error.status = 404;
				throw error;
			}

			room = rooms.get(roomId);

			next();
		});
*/
  //MS_lOGD("initBridgeSignalChannel(expressApp)");
  //initBridgeSignalChannel(null,rooms);
	/**
	 * API GET resource that returns the mediasoup Router RTP capabilities of
	 * the room->
	 */
	if(
		("/rooms/"+roomId ) ==  path )
		{
			auto data = room->getRouterRtpCapabilities();

			respdata = data ;
    }else

	/**
	 * POST API to create a Broadcaster.
	 */
	if(
		("/rooms/"+roomId+"/broadcasters") ==  path )
		{
			auto id = body["id"];
			auto displayName = body["displayName"];
			auto device = body["device"];
			auto rtpCapabilities = body["rtpCapabilities"];
      MS_lOGD("[LiveRoom] get req body=${JSON.stringify(body)}");
			try
			{
                RtpCapabilities cap = rtpCapabilities;
				auto data = room->createBroadcaster(
					
						id,
						displayName,
						device,
                        cap
					);

				respdata = data ;
			}
			catch (...)
			{
				
			}
    }else

	/**
	 * DELETE API to delete a Broadcaster.
	 */
	if(
		("/rooms/"+roomId+"/broadcasters/:broadcasterId") ==  path )
		{
			auto broadcasterId = params["broadcasterId"];

			room->deleteBroadcaster( broadcasterId );

			//res.status(200).send("broadcaster deleted");
    }else

	/**
	 * POST API to create a mediasoup Transport associated to a Broadcaster.
	 * It can be a PlainTransport or a WebRtcTransport depending on the
	 * type parameters in the body. There are also additional parameters for
	 * PlainTransport.
	 */
	if(
		("/rooms/"+roomId+"/broadcasters/:broadcasterId/transports") ==  path )
		{
			auto broadcasterId = params["broadcasterId"];
			auto type = body["type"];
              auto rtcpMux = body["rtcpMux"];
              auto comedia = body["comedia"];
              auto sctpCapabilities = body["sctpCapabilities"];

			try
			{
                SctpCapabilities sctp = sctpCapabilities;
				auto data = room->createBroadcasterTransport(
					
						broadcasterId,
						type,
						rtcpMux,
						comedia, 
                        sctp
					);

				respdata = data ;
			}
			catch (...)
			{
				
			}
		}else

	/**
	 * POST API to connect a Transport belonging to a Broadcaster. Not needed
	 * for PlainTransport if it was created with comedia option set to true.
	 */
	if(
		("/rooms/"+roomId+"/broadcasters/:broadcasterId/transports/:transportId/connect")  ==  path )
		{
              auto broadcasterId = params["broadcasterId"];
              auto transportId = params["transportId"];
            DtlsParameters dtlsParameters  = body["dtlsParameters"];

			try
			{
                
				room->connectBroadcasterTransport(
					
						broadcasterId,
						transportId,
						dtlsParameters
					);

				//respdata = data ;
			}
			catch (...)
			{
				
			}
		}else

	/**
	 * POST API to create a mediasoup Producer associated to a Broadcaster.
	 * The exact Transport in which the Producer must be created is signaled in
	 * the URL path. Body parameters include kind and rtpParameters of the
	 * Producer.
	 */
	if(
		("/rooms/"+roomId+"/broadcasters/:broadcasterId/transports/:transportId/producers")  ==  path )
		{
			auto broadcasterId = params["broadcasterId"];
          auto transportId = params["transportId"];
          auto kind = body["kind"];
            RtpParameters rtpParameters = body["rtpParameters"];
			//auto { kind, rtpParameters } = body;

			try
			{
				auto data = room->createBroadcasterProducer(
					
						broadcasterId,
						transportId,
						kind,
						rtpParameters
					);

				respdata = data ;
			}
			catch (...)
			{
				
			}
		}else

	/**
	 * POST API to create a mediasoup Consumer associated to a Broadcaster.
	 * The exact Transport in which the Consumer must be created is signaled in
	 * the URL path. Query parameters must include the desired producerId to
	 * consume.
	 */
	if(
		("/rooms/"+roomId+"/broadcasters/:broadcasterId/transports/:transportId/consume") ==  path )
		{
		  auto broadcasterId = params["broadcasterId"];
          auto transportId = params["transportId"];
          auto producerId = query["producerId"];
		//	auto { producerId } = req.query;

			try
			{
				auto data = room->createBroadcasterConsumer(
					
						broadcasterId,
						transportId,
						producerId
					);

				respdata = data ;
			}
			catch (...)
			{
				
			}
		}else

	/**
	 * POST API to create a mediasoup DataConsumer associated to a Broadcaster.
	 * The exact Transport in which the DataConsumer must be created is signaled in
	 * the URL path. Query body must include the desired producerId to
	 * consume.
	 */
	if(
		("/rooms/"+roomId+"/broadcasters/:broadcasterId/transports/:transportId/consume/data") ==  path )
		{
			auto broadcasterId = params["broadcasterId"];
            auto transportId = params["transportId"];
			auto dataProducerId = body["dataProducerId"];

			try
			{
				auto data = room->createBroadcasterDataConsumer(
					
						broadcasterId,
						transportId,
						dataProducerId
					);

				respdata = data ;
			}
			catch (...)
			{
				
			}
		}else
	
	/**
	 * POST API to create a mediasoup DataProducer associated to a Broadcaster.
	 * The exact Transport in which the DataProducer must be created is signaled in
	 */
	if(
		("/rooms/"+roomId+"/broadcasters/:broadcasterId/transports/:transportId/produce/data") ==  path )
		{
			auto broadcasterId = params["broadcasterId"];
              auto transportId = params["transportId"];
              auto label = body["label"];
              auto protocol = body["protocol"];
              SctpStreamParameters sctpStreamParameters = body["sctpStreamParameters"];
              auto appData = body["appData"];

			try
			{
				auto data = room->createBroadcasterDataProducer(
					
						broadcasterId,
						transportId,
						label,
						protocol,
						sctpStreamParameters,
						appData
					);

				respdata = data ;
			}
			catch (...)
			{
				
			}
		}else



	if(
		"/describeSignalingChannel" ==  path )
		{
			/*
			* ScaryTestChannel,
		Master,
		us-west-2,
		arn:aws:kinesisvideo:us-west-2:848161169354:channel/ScaryTestChannel/1606901298484,
		https://r-fd57ec7b.kinesisvideo.us-west-2.amazonaws.com,
		wss://m-75621fd6.kinesisvideo.us-west-2.amazonaws.com,1607436362
			* */
			auto roomId= body["CorrelationId"] ;
			auto bridgeId = body["RecipientClientId"];
			auto MessagePayload = body["MessagePayload"];
			MS_lOGD("describeSignalingChannel recv data=%s",body.dump().c_str());
			json ChannelInfo1 = {
                    { "ChannelInfo" , {
                            {"ChannelARN" , "arn:aws:kinesisvideo:us-west-2:848161169354:channel/1234569/1606901298484"},
                            {"ChannelName" , roomId},
                            {"Version" , "v0"},
                            {"ChannelStatus" , "CREATING"},
                            {"ChannelType" , "SINGLE_MASTER"},
                            {"CreationTime" , ""},
                        }
                    }
			};
			//just create the room with roomId
			getOrCreateRoom(roomId);
			// The room must exist for all API requests.
			if (!rooms[roomId])
			{
				//auto error = new Error("room with id roomId not found");

				//error.status = 404;
				//throw error;
			}

            room = rooms[roomId];

			try
			{

				auto sdpOffer = getStringFromBase64(MessagePayload);
				MS_lOGD("messagePayload=%s",sdpOffer.dump().c_str());
				auto room = rooms[roomId];
                auto remoteCaps = mediasoupclient::Sdp::Offer::getMediasoupRtpCapabilities(sdpOffer["sdp"],room->getLocalSdp());
                json device ={
                    {"name",bridgeId},
                    {"rtpCapabilities",remoteCaps }
                };
                std::string displayName = bridgeId;
                room->createBridge(bridgeId,displayName,device,remoteCaps);
                SctpCapabilities sctp;
				auto data = room->createBridgeTransport(

						/*bridgeId:*/bridgeId,
						/*type:*/"webrtc",
						/*rtcpMux:*/false,
						/*comedia:*/true,
                        sctp
					);
				MS_lOGD("createBridgeTransport transportId=%s",data["id"].dump().c_str());


			}
			catch (...)
			{
				MS_lOGE("error");
			}

			//res.status(200).json(ChannelInfo1);
		}else
	if(
		"/getSignalingChannelEndpoint" ==  path )
		{
			MS_lOGD("getSignalingChannelEndpoint recv  data=",body.dump().c_str());
			json ResourceEndpointList = {
                "ResourceEndpointList" , {
					{
						{"Protocol", "http"},
						//{"ResourceEndpoint", "http://127.0.0.1:"+config.http.listenPort},
						{"Version", "v0"},
						{"ChannelStatus","CREATING"},
						{"ChannelType", "SINGLE_MASTER"},
						{"CreationTime", ""},
					},
					{
						{"Protocol", "ws"},
					//	{"ResourceEndpoint", "ws://127.0.0.1:"+config.http.listenPort)+1)+"/"},
						{"Version", "v0"},
						{"ChannelStatus", "CREATING"},
						{"ChannelType", "SINGLE_MASTER"},
						{"CreationTime", ""},
					}
                }
			};
			//res.status(200).json(ResourceEndpointList);
		}else
	if(
		"/v1/get-ice-server-config" ==  path )
		{
			/*
			* {"IceServerList":[{"Password":"H8TZ5pAjOkalNVQJ5UgQt3Godj3URm/3lsfcJYLMwOs=","Ttl":300,"Uris":["turn:54-188-75-37.t-67e8ec01.kinesisvideo.us-west-2.amazonaws.com:443?transport=udp","turns:54-188-75-37.t-67e8ec01.kinesisvideo.us-west-2.amazonaws.com:443?transport=udp","turns:54-188-75-37.t-67e8ec01.kinesisvideo.us-west-2.amazonaws.com:443?transport=tcp"],"Username":"1610088702:djE6YXJuOmF3czpraW5lc2lzdmlkZW86dXMtd2VzdC0yOjg0ODE2MTE2OTM1NDpjaGFubmVsL1NjYXJ5VGVzdENoYW5uZWwvMTYwNjkwMTI5ODQ4NA=="},{"Password":"7Kw5G4K9SpLYEvibvZiTg48uoa5vJ7X3I/MxCkmBqAs=","Ttl":300,"Uris":["turn:34-220-69-168.t-67e8ec01.kinesisvideo.us-west-2.amazonaws.com:443?transport=udp","turns:34-220-69-168.t-67e8ec01.kinesisvideo.us-west-2.amazonaws.com:443?transport=udp","turns:34-220-69-168.t-67e8ec01.kinesisvideo.us-west-2.amazonaws.com:443?transport=tcp"],"Username":"1610088702:djE6YXJuOmF3czpraW5lc2lzdmlkZW86dXMtd2VzdC0yOjg0ODE2MTE2OTM1NDpjaGFubmVsL1NjYXJ5VGVzdENoYW5uZWwvMTYwNjkwMTI5ODQ4NA=="}]}
			* */
			MS_lOGD("/v1/get-ice-server-config recv data=",body.dump().c_str());
			json IceServerList = {
                "IceServerList",{
                        {
                            {"Password" , "H8TZ5pAjOkalNVQJ5UgQt3Godj3URm/3lsfcJYLMwOs="},
                            {"Ttl" , 300},
                            {"Uris" , {
                                "turn:192.168.1.192?transport=udp",
                                "turns:192.168.1.192?transport=udp"
                            },
                            {"Username" , "aa"}
                        }
                    }
                }
			};
			room = rooms["1234569"];
			//room->websocket = webSocketClient;
			//res.status(200).json(IceServerList);
		}

}

/**
 * Create a protoo WebSocketServer to allow WebSocket connections from browsers.
 */
void Server::runProtooWebSocketServer()
{
	MS_lOGD("running protoo WebSocketServer...");
	// Handle connections from clients.

    protooWebsockServer->on("connectionrequest", [&](std::string&& roomid, std::string&& peerid, WebSocketTransport* transport)
	{
        //MS_lOGD("[server] on connectionrequest info=${JSON.stringify(info)}")
		// The client indicates the roomId and peerId in the URL query.
		//auto u = url.parse(info.request.url, true);
        auto roomId = roomid;//u.query["roomId"];
        auto peerId = peerid;//u.query["peerId"];

    	auto dvr = true;//u.query["dvr"];

		if (roomId.empty() || peerId.empty())
		{
			//reject(400, "Connection request without roomId and/or peerId");

			return;
		}

		MS_lOGD(
			"protoo connection request [roomId:%s, peerId:%s]",
			roomId.c_str(), peerId.c_str());

		// Serialize this code into the queue to avoid that two peers connecting at
		// the same time with the same roomId create two separate rooms with same
		// roomId.
        auto room = getOrCreateRoom(roomId );
          if(dvr)  {
            room->dvr = true;
          }
            

        // Accept the protoo WebSocket connection.
       // auto protooWebSocketTransport = accept(json({}));

       room->handleProtooConnection(peerId,nullptr, transport);
        
//		queue.push(async () =>
//		{
//			auto room = getOrCreateRoom(roomId );
//              if(dvr)  {
//                room->dvr = true;
//              }
//
//
//			// Accept the protoo WebSocket connection.
//			auto protooWebSocketTransport = accept();
//
//			room->handleProtooConnection(peerId, protooWebSocketTransport);
//		})
//			.catch((error) =>
//			{
//				logger.error("room creation or room joining failed:%o", error);
//
//				reject(error);
//			});
	});
    protooWebsockServer->runWebSocketServer();
}
/**
 * Get a Room instance (or create one if it does not exist).
 */
std::shared_ptr<Room> Server::getOrCreateRoom(std::string roomId)
{
	auto room = rooms[roomId];

	// If the Room does not exist create a new one.
	if (!room)
	{
		MS_lOGD("creating a new Room [roomId:%s]", roomId.c_str());

		auto mediasoupWorker = getMediasoupWorker();
    //room->setConfig(config);
		room = Room::create( mediasoupWorker, roomId );
   
		rooms[roomId] = room;
		room->on("close",[&]()
        {
			MS_lOGD("getOrCreateRoom rooms delete [roomId:%s]", roomId.c_str());
			rooms.erase(roomId);
		});
	}

	return room;
}
void Server::initMediasoup()
{
    mediasoup = mediasoup::CreateMediasoup();
    mediasoup->Init();
	mediasoup::RtpCapabilities rtpCapabilities = mediasoup->GetSupportedRtpCapabilities();
	for (auto & item : rtpCapabilities.headerExtensions) {
		//std::cout << "headerExtensions.uri:" << item.uri << std::endl;
	}

}
void Server::initWorker(int consumerFd,int producerFd,int payloadConsumerFd,int payloadProducerFd)
{
    workerSettings.consumerFd=consumerFd;
    workerSettings.producerFd=producerFd;
    workerSettings.payloadConsumerFd=payloadConsumerFd;
    workerSettings.payloadProducerFd=payloadProducerFd;
  	auto worker = mediasoup->CreateWorker(&myWorkerObserver, workerSettings);
    workers.push_back(worker);
}
json Server::getStringFromBase64(std::string payload)
{
    Base64 base;
    auto dec = base.Decode(payload.c_str(),payload.length());
    return json::parse(dec);
}
void Server::testProtoo()
{
    MS_lOGD("running protoo WebSocketServer...");
    // Handle connections from clients.
   // protooWebsockServer->on("connectionrequest", [&](std::string& roomid, std::string& peerid, WebSocketTransport* transport)
    if(true)
    {
        MS_lOGD("[server] on connectionrequest info=${JSON.stringify(info)}")
        // The client indicates the roomId and peerId in the URL query.
        //auto u = url.parse(info.request.url, true);
        std::string roomId = "1234569";//roomid;//u.query["roomId"];
        std::string peerId = "edu123";//peerid;//u.query["peerId"];
        
        //WebSocketTransport* transport = new WebSocketTransport();
        auto dvr = true;//u.query["dvr"];
        
        if (roomId.empty() || peerId.empty())
        {
            //reject(400, "Connection request without roomId and/or peerId");

            return;
        }

        MS_lOGD(
            "protoo connection request [roomId:%s, peerId:%s]",
            roomId.c_str(), peerId.c_str());

        // Serialize this code into the queue to avoid that two peers connecting at
        // the same time with the same roomId create two separate rooms with same
        // roomId.
        auto room = getOrCreateRoom(roomId );
          if(dvr)  {
            room->dvr = true;
          }
            

        // Accept the protoo WebSocket connection.
       // auto protooWebSocketTransport = accept(json({}));

        room->handleProtooConnection(peerId,nullptr, nullptr);
        
//        queue.push(async () =>
//        {
//            auto room = getOrCreateRoom(roomId );
//              if(dvr)  {
//                room->dvr = true;
//              }
//
//
//            // Accept the protoo WebSocket connection.
//            auto protooWebSocketTransport = accept();
//
//            room->handleProtooConnection(peerId, protooWebSocketTransport);
//        })
//            .catch((error) =>
//            {
//                logger.error("room creation or room joining failed:%o", error);
//
//                reject(error);
//            });
    }
}
                                   
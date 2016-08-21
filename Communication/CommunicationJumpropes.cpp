#include "CommunicationJumpropes.h"

#include <Jumpropes/ThreadedConnection.h>
#include <Groundfloor/Materials/Functions.h>
#include <Groundfloor/Bookshelfs/BValue.h>
#include <Jumpropes/ClientSocket.h>
#include "../System/Configuration.h"
#include <iostream>
#include <fstream>

#include "../System/Modules.h"

AudacityRover::CommunicationJumpropes::CommunicationJumpropes() : OpenALRF::ICommunication()
{
   this->Server.startListening(Configuration::Instance()->CommandServerPort);
}

void AudacityRover::CommunicationJumpropes::LoadFromBackLog()
{
   incomingdata = "";
}

//void AudacityRover::CommunicationJumpropes::SaveToBackLog()
//{
//}

void AudacityRover::CommunicationJumpropes::Process()
{
   // todo
}

void AudacityRover::CommunicationJumpropes::SendToStation(const std::string AMessage)
{
   Jumpropes::ClientSocket Client;
   InitializeStationConnection(Client);
   Client.connect();

   Groundfloor::String Data = AMessage;
   Groundfloor::CommReturnData ErrorInfo;

   if (!Client.send(&Data, &ErrorInfo))
   {
      std::cerr << "Failed to send data to " << this->Server.LastSender << " (" << ErrorInfo.errorcode << ")" << std::endl;
   }

   Client.disconnect();
}

void AudacityRover::CommunicationJumpropes::InitializeStationConnection(Jumpropes::ClientSocket &Client)
{
   Client.remotePort.set(14666);
   Client.getRemoteAddress()->ip = this->Server.LastSender;
}

std::string AudacityRover::CommunicationJumpropes::GetStatusInfo()
{
   return "";
}

void AudacityRover::Receiver::newClientConnection(Jumpropes::BaseSocket * aClient)
{
   LastSender = aClient->getRemoteAddress()->ip.getValue();

   Connection *Conn = new Connection(aClient, this);
   Clients.addElement(Conn);
}

AudacityRover::Receiver::Receiver() : Jumpropes::ThreadedServer()
{
}

void AudacityRover::Connection::newMessageReceived(const String * sMessage)
{
   if (sMessage->startsWith_ansi("BINCMD") || sMessage->startsWith_ansi("BATCHCMD"))
   {
      // resets remaining buffer
      ReceptionBuffer.setValue(sMessage);
   }
   else if (sMessage->startsWith_ansi("GET /"))
   {
      int httppos = sMessage->pos_ansi("HTTP");
      Groundfloor::String url(sMessage->getPointer(4) + 1, httppos - 6);
      std::cout << url.getValue() << std::endl;

      OpenALRF::Command StrCmd{OpenALRF::modVoid, OpenALRF::actVoid, 0, 0, ""};

      Groundfloor::Vector SplitURL;
      Groundfloor::split_p(&SplitURL, &url, "/", 5);
      Groundfloor::BValue Val;

      if (SplitURL.size() >= 2)
      {
         Val.setString(static_cast<Groundfloor::String *>(SplitURL.elementAt(0)));

         if (Val.asString()->match_ansi("web"))
         {
            Val.setString(static_cast<Groundfloor::String *>(SplitURL.elementAt(1)));
            
            ReplyWithFile(Val.asString());
         }
         else
         {
            StrCmd.Module = static_cast<OpenALRF::module_t>(Val.asInteger());

            Val.setString(static_cast<Groundfloor::String *>(SplitURL.elementAt(1)));
            StrCmd.Action = static_cast<OpenALRF::action_t>(Val.asInteger());

            if (SplitURL.size() >= 3)
            {
               Val.setString(static_cast<Groundfloor::String *>(SplitURL.elementAt(2)));
               StrCmd.param1 = static_cast<int16_t>(Val.asInteger());

               if (SplitURL.size() >= 4)
               {
                  Val.setString(static_cast<Groundfloor::String *>(SplitURL.elementAt(3)));
                  StrCmd.param2 = static_cast<int16_t>(Val.asInteger());

                  if (SplitURL.size() == 5)
                  {
                     StrCmd.param3 = static_cast<Groundfloor::String *>(SplitURL.elementAt(4))->getValue();
                  }
               }
            }

            Modules::Instance()->CommandQueue->Add(StrCmd);

            ReplyHTTPOK();
         }
      }
      else
      {
         ReplyHTTPFail();
      }
   }
   else
   {
      ReceptionBuffer.append(sMessage);
   }

   if (ReceptionBuffer.getLength() > 0)
   {
      auto BinCmd = this->ReadNextCommand(&ReceptionBuffer);
      while (BinCmd.Cmd.Action != OpenALRF::actVoid)
      {
         if (BinCmd.Order == 0)
         {
            Modules::Instance()->CommandQueue->Add(BinCmd.Cmd);
         }
         else
         {
            OrderedCommandBuffer.push_back(BinCmd);

            // todo: sort buffer, remove duplicates and figure out when to actually commandqueue it
         }

         BinCmd = this->ReadNextCommand(&ReceptionBuffer);
      }
   }
}

OpenALRF::OrderedCommand AudacityRover::Connection::ReadNextCommand(String * AData)
{
   OpenALRF::OrderedCommand BinCmd{ 0, OpenALRF::ordAny, { OpenALRF::modVoid, OpenALRF::actVoid, 0, 0, "" } };

   unsigned skipcount = 0;

   char *msg = nullptr;
   if (AData->startsWith_ansi("BINCMD"))
   {
      msg = AData->getPointer(6);
      skipcount = 6;
   }
   else if (AData->startsWith_ansi("BATCHCMD"))
   {
      msg = AData->getPointer(8);
      BinCmd.Order = (msg[0] << 8) | msg[1];
      BinCmd.Type = OpenALRF::ordAny;

      msg = AData->getPointer(10);
      skipcount = 9;
   }
   else if (AData->startsWith_ansi("BATCHBEG"))
   {
      msg = AData->getPointer(8);
      BinCmd.Order = (msg[0] << 8) | msg[1];
      BinCmd.Type = OpenALRF::ordStart;

      msg = AData->getPointer(10);
      skipcount = 9;
   }
   else if (AData->startsWith_ansi("BATCHEND"))
   {
      msg = AData->getPointer(8);
      BinCmd.Order = (msg[0] << 8) | msg[1];
      BinCmd.Type = OpenALRF::ordStop;

      msg = AData->getPointer(10);
      skipcount = 9;
   }
   else
   {
      msg = AData->getPointer(0);
   }

   uint32_t cmdlen = (msg[0] << 24) | (msg[1] << 16) | (msg[2] << 8) | msg[3];

   if (cmdlen >= 8)
   {
      BinCmd.Cmd.Module = static_cast<OpenALRF::module_t>(msg[4] << 8 | msg[5]);
      BinCmd.Cmd.Action = static_cast<OpenALRF::action_t>(msg[6] << 8 | msg[7]);

      if (cmdlen >= 10)
      {
         BinCmd.Cmd.param1 = static_cast<int16_t>(msg[8] << 8 | msg[9]);
         if (cmdlen >= 12)
         {
            BinCmd.Cmd.param2 = static_cast<int16_t>(msg[10] << 8 | msg[11]);

            BinCmd.Cmd.param3 = "";
            if (cmdlen > 12)
            {
               Groundfloor::String StrParam(sMessage->getPointer(12), cmdlen - 12);
               BinCmd.Cmd.param3.append(StrParam.getValue(), StrParam.getLength());
            }
         }
      }

      skipcount += cmdlen;

      AData->remove(0, skipcount);
   }

   return BinCmd;
}

void AudacityRover::Connection::ReplyBinaryOK()
{
   Groundfloor::String Understood("OK");
   this->socket->send(&Understood);
}

void AudacityRover::Connection::ReplyBinaryFail()
{
   Groundfloor::String Understood("FAIL");
   this->socket->send(&Understood);
}

void AudacityRover::Connection::ReplyHTTPOK()
{
   Groundfloor::String Understood("HTTP/1.1 200 OK\n\nOK\n");
   this->socket->send(&Understood);
   this->socket->disconnect();
}

void AudacityRover::Connection::ReplyHTTPFail()
{
   Groundfloor::String Error("HTTP/1.1 404 Error\n\nError\n");
   this->socket->send(&Error);
   this->socket->disconnect();
}

void AudacityRover::Connection::ReplyWithFile(const String * AFile)
{
   if (!Groundfloor::FileExists(AFile))
   {
      ReplyHTTPFail();
   }
   else
   {
      auto Data = GetHTTPHeaderForFile(AFile);

      std::ifstream file;
      file.open(AFile->getValue(), std::ifstream::in | std::ifstream::binary);

      file.seekg(0, file.end);
      file.seekg(0, file.beg);

      char buffer[1024];
      file.read(buffer, 1024);
      auto bytesread = file.gcount();
      while (bytesread > 0)
      {
         Data.append(buffer, bytesread);

         file.read(buffer, 1024);
         bytesread = file.gcount();
      }

      this->socket->send(&Data);
      while (!this->socket->isReadyToSend()) {
         GFMillisleep(1);
      }
      this->socket->disconnect();
   }
}

Groundfloor::String AudacityRover::Connection::GetHTTPHeaderForFile(const Groundfloor::String * AFile)
{
   Groundfloor::String Data("HTTP/1.1 200 OK\n");

   Data.append_ansi("Content-Length: ");

   Groundfloor::BValue ContentLength;
   ContentLength.setInteger(Groundfloor::GetFileSize(AFile));

   Data.append(ContentLength.asString());

   if (AFile->endsWith_ansi(".jpeg") || AFile->endsWith_ansi(".jpg"))
   {
      Data.append_ansi("\nContent-Type: image/jpeg\n\n");
   }
   else if (AFile->endsWith_ansi(".bmp"))
   {
      Data.append_ansi("\nContent-Type: image/bmp\n\n");
   }
   else if (AFile->endsWith_ansi(".txt"))
   {
      Data.append_ansi("\nContent-Type: text/plain\n\n");
   }
   else
   {
      Data.append_ansi("\nContent-Type: application/octet-stream\n\n");
   }

   return Data;
}

AudacityRover::Connection::Connection(Jumpropes::BaseSocket * AClient, Receiver *AReceiver) : Jumpropes::ThreadedConnection(AClient)
{
   this->Server = AReceiver;
   this->start();
}


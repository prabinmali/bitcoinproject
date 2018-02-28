#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdint.h>
//#include <gsl/gsl_rng.h>
//#include <gsl/gsl_randist.h>
#include <json-c/json.h>
#include <gsl/gsl_rng.h>

#include "./simulator/event.h"
#include "./simulator/initialize.h"
#include "./utils/heap.h"
#include "./utils/hashTable.h"
#include "./gc-7.2/include/gc.h"
#include "./utils/array.h"
#include "./protocol/findRoute.h"
#include "./protocol/protocol.h"
#include "./simulator/stats.h"


/*void printBalances() {
  long i, j, *channelID;
  Peer* peer;
  Array* peerChannels;
  Channel* channel;

  printf("PRINT BALANCES\n");
  for(i=0; i<peerIndex; i++) {
    peer = hashTableGet(peers, i);
    peerChannels = peer->channel;
    for(j=0; j<arrayLen(peerChannels); j++) {
      channelID = arrayGet(peerChannels, j);
      channel = hashTableGet(channels, *channelID);
      printf("Peer %ld, Channel %ld, Balance %lf\n", peer->ID, channel->channelInfoID, channel->balance);
    }
  }
  }

void printPayments() {
  long i, j;
  Payment *payment;
  Array* routeHops;
  RouteHop* hop;
  Peer* peer;
  Channel* forward, *backward;

  for(i = 0; i < paymentIndex; i++) {
    payment = hashTableGet(payments, i);
    printf("PAYMENT %ld\n", payment->ID);
    if(payment->route==NULL) continue;
    routeHops = payment->route->routeHops;
    for(j=0; j<arrayLen(routeHops); j++){
      hop = arrayGet(routeHops, j);
      peer = hashTableGet(peers, hop->pathHop->sender);
      if(isPresent(hop->pathHop->channel, peer->channel)) {
        forward = hashTableGet(channels, hop->pathHop->channel);
        backward = hashTableGet(channels, forward->otherChannelDirectionID);
        printf("Sender %ld, Receiver %ld, Channel %ld, Balance forward %ld, Balance backward %ld\n",
               hop->pathHop->sender, hop->pathHop->receiver, forward->channelInfoID, forward->balance, backward->balance);

      }
      else {
        forward = hashTableGet(channels, hop->pathHop->channel);
        printf("Sender %ld, Receiver %ld, Channel %ld, Channel closed\n", hop->pathHop->sender, hop->pathHop->receiver, forward->channelInfoID);
      }
    }
  }
}

struct json_object* jsonNewChannelDirection(Channel* direction) {
  struct json_object* jdirection;
  struct json_object* jID, *jcounterpartyID, *jbalance, *jpolicy, *jfeebase, *jfeeprop, *jtimelock;

  jdirection = json_object_new_object();

  jID = json_object_new_int64(direction->ID);
  jcounterpartyID = json_object_new_int64(direction->counterparty);
  jbalance = json_object_new_int64(direction->balance);

  jpolicy = json_object_new_object();
  jfeebase = json_object_new_int64(direction->policy.feeBase);
  jfeeprop = json_object_new_int64(direction->policy.feeProportional);
  jtimelock = json_object_new_int(direction->policy.timelock);
  json_object_object_add(jpolicy, "FeeBase", jfeebase );
  json_object_object_add(jpolicy, "FeeProportional", jfeeprop);
  json_object_object_add(jpolicy, "Timelock", jtimelock );
 
  json_object_object_add(jdirection, "ID", jID );
  json_object_object_add(jdirection, "CounterpartyID", jcounterpartyID);
  json_object_object_add(jdirection, "Balance", jbalance );
  json_object_object_add(jdirection, "Policy", jpolicy );

  return jdirection;
}

struct json_object* jsonNewChannel(ChannelInfo *channel) {
  struct json_object* jchannel;
  struct json_object* jID, *jcapacity, *jlatency, *jpeer1, *jpeer2, *jdirection1, *jdirection2;

  jchannel = json_object_new_object();

  jID = json_object_new_int64(channel->ID);
  jpeer1 = json_object_new_int64(channel->peer1);
  jpeer2 = json_object_new_int64(channel->peer2);
  jcapacity = json_object_new_int64(channel->capacity);
  jlatency = json_object_new_int(channel->latency);
  jdirection1 = jsonNewChannelDirection(hashTableGet(channels, channel->channelDirection1));
  jdirection2 = jsonNewChannelDirection(hashTableGet(channels, channel->channelDirection2));

  json_object_object_add(jchannel, "ID", jID);
  json_object_object_add(jchannel, "Peer1ID", jpeer1);
  json_object_object_add(jchannel, "Peer2ID", jpeer2);
  json_object_object_add(jchannel, "Capacity", jcapacity);
  json_object_object_add(jchannel, "Latency", jlatency);
  json_object_object_add(jchannel, "Direction1", jdirection1);
  json_object_object_add(jchannel, "Direction2", jdirection2);

  return jchannel;
}

struct json_object* jsonNewPeer(Peer *peer) {
  struct json_object* jpeer;
  struct json_object *jID, *jChannelSize, *jChannelIDs, *jChannelID, *jwithholdsR;
  long i, *channelID;

  jpeer = json_object_new_object();

  jID = json_object_new_int64(peer->ID);
  jChannelSize = json_object_new_int64(arrayLen(peer->channel));
  jwithholdsR = json_object_new_int(peer->withholdsR);

  jChannelIDs = json_object_new_array();
  for(i=0; i<arrayLen(peer->channel); i++) {
    channelID = arrayGet(peer->channel, i);
    jChannelID = json_object_new_int64(*channelID);
    json_object_array_add(jChannelIDs, jChannelID);
  }

  json_object_object_add(jpeer, "ID", jID);
  json_object_object_add(jpeer,"ChannelsSize",jChannelSize);
  json_object_object_add(jpeer, "WithholdsR", jwithholdsR);
  json_object_object_add(jpeer, "ChannelIDs", jChannelIDs);

  return jpeer;
}

void jsonWriteInput() {
  long i;
  struct json_object* jpeer, *jpeers, *jchannel, *jchannels;
  struct json_object* jobj;
  Peer* peer;
  ChannelInfo* channel;

  jobj = json_object_new_object();
  jpeers = json_object_new_array();
  jchannels = json_object_new_array();

  for(i=0; i<peerIndex; i++) {
    peer = hashTableGet(peers, i);
    jpeer = jsonNewPeer(peer);
    json_object_array_add(jpeers, jpeer);
  }
  json_object_object_add(jobj, "Peers",jpeers);

  for(i=0; i<channelInfoIndex; i++){
    channel = hashTableGet(channelInfos, i);
    jchannel = jsonNewChannel(channel);
    json_object_array_add(jchannels, jchannel);
  }
  json_object_object_add(jobj, "Channels", jchannels);

  //  printf("%s\n", json_object_to_json_string(jobj));

  json_object_to_file_ext("simulator_input.json", jobj, JSON_C_TO_STRING_PRETTY);

}

void csvWriteInput() {
  FILE *csvPeer, *csvChannelInfo, *csvChannel;
  Peer* peer;
  ChannelInfo* channelInfo;
  Channel* channel;
  long i, j, *channelID;

  csvPeer = fopen("peer.csv", "w");
   if(csvPeer==NULL) {
    printf("ERROR cannot open file peer.csv\n");
    return;
  }

  fprintf(csvPeer, "ID,ChannelsSize,WithholdsR,ChannelIDs\n");
  for(i=0; i<peerIndex; i++) {
    peer = hashTableGet(peers, i);
    fprintf(csvPeer, "%ld,%ld,%d,", peer->ID, arrayLen(peer->channel), peer->withholdsR);
    for(j=0; j<arrayLen(peer->channel); j++) {
      channelID = arrayGet(peer->channel, j);
      if(j==arrayLen(peer->channel)-1) 
        fprintf(csvPeer,"%ld\n", *channelID);
      else
        fprintf(csvPeer,"%ld-", *channelID);
    }
  }


  fclose(csvPeer);
 
  csvChannelInfo = fopen("channelInfo.csv", "w");
  if(csvChannelInfo==NULL) {
    printf("ERROR cannot open file channelInfo.csv\n");
    return;
  }

  fprintf(csvChannelInfo, "ID,Peer1,Peer2,Direction1,Direction2,Capacity,Latency\n");
  for(i=0; i<channelInfoIndex; i++) {
    channelInfo = hashTableGet(channelInfos, i);
    fprintf(csvChannelInfo,"%ld,%ld,%ld,%ld,%ld,%ld,%d\n",channelInfo->ID, channelInfo->peer1, channelInfo->peer2, channelInfo->channelDirection1, channelInfo->channelDirection2, channelInfo->capacity, channelInfo->latency);
  }

  fclose(csvChannelInfo);

  csvChannel = fopen("channel.csv", "w");
  if(csvChannel==NULL) {
    printf("ERROR cannot open file channel.csv\n");
    return;
  }


  fprintf(csvChannel, "ID,ChannelInfo,OtherDirection,Counterparty,Balance,FeeBase,FeeProportional,Timelock\n");
  for(i=0; i<channelIndex; i++) {
    channel = hashTableGet(channels, i);
    fprintf(csvChannel, "%ld,%ld,%ld,%ld,%ld,%d,%d,%d\n", channel->ID, channel->channelInfoID, channel->otherChannelDirectionID, channel->counterparty, channel->balance, channel->policy.feeBase, channel->policy.feeProportional, channel->policy.timelock);
  }


  fclose(csvChannel);

}

*/

void csvWriteOutput() {
  FILE* csvChannelInfoOutput, *csvChannelOutput, *csvPaymentOutput;
  long i,j, *ignored;
  ChannelInfo* channelInfo;
  Channel* channel;
  Payment* payment;
  Route* route;
  Array* hops;
  RouteHop* hop;

  csvChannelInfoOutput = fopen("channelInfo_output.csv", "w");
  if(csvChannelInfoOutput  == NULL) {
    printf("ERROR cannot open channelInfo_output.csv\n");
    return;
  }
  fprintf(csvChannelInfoOutput, "ID,Direction1,Direction2,Peer1,Peer2,Capacity,Latency,IsClosed\n");

  for(i=0; i<channelInfoIndex; i++) {
    channelInfo = hashTableGet(channelInfos, i);
    fprintf(csvChannelInfoOutput, "%ld,%ld,%ld,%ld,%ld,%ld,%d,%d\n", channelInfo->ID, channelInfo->channelDirection1, channelInfo->channelDirection2, channelInfo->peer1, channelInfo->peer2, channelInfo->capacity, channelInfo->latency, channelInfo->isClosed);
  }

  fclose(csvChannelInfoOutput);

  csvChannelOutput = fopen("channel_output.csv", "w");
  if(csvChannelInfoOutput  == NULL) {
    printf("ERROR cannot open channel_output.csv\n");
    return;
  }
  fprintf(csvChannelOutput, "ID,ChannelInfo,OtherDirection,Counterparty,Balance,FeeBase,FeeProportional,Timelock, isClosed\n");

  for(i=0; i<channelIndex; i++) {
    channel = hashTableGet(channels, i);
    fprintf(csvChannelOutput, "%ld,%ld,%ld,%ld,%ld,%d,%d,%d,%d\n", channel->ID, channel->channelInfoID, channel->otherChannelDirectionID, channel->counterparty, channel->balance, channel->policy.feeBase, channel->policy.feeProportional, channel->policy.timelock, channel->isClosed);
  }

  fclose(csvChannelOutput);

  csvPaymentOutput = fopen("payment_output.csv", "w");
  if(csvChannelInfoOutput  == NULL) {
    printf("ERROR cannot open payment_output.csv\n");
    return;
  }
  fprintf(csvPaymentOutput, "ID,Sender,Receiver,Amount,Time,IsSuccess,IsAPeerUncoop,Route,IgnoredPeers,IgnoredChannels\n");

  for(i=0; i<paymentIndex; i++)  {
    payment = hashTableGet(payments, i);
    fprintf(csvPaymentOutput, "%ld,%ld,%ld,%ld,%ld,%d,%d,", payment->ID, payment->sender, payment->receiver, payment->amount, payment->startTime, payment->isSuccess, payment->isAPeerUncoop);
    route = payment->route;

    if(route==NULL)
      fprintf(csvPaymentOutput, "-1");
    else {
      hops = route->routeHops;
      for(j=0; j<arrayLen(hops); j++) {
        hop = arrayGet(hops, j);
        if(j==arrayLen(hops)-1)
          fprintf(csvPaymentOutput,"%ld",hop->pathHop->channel);
        else
          fprintf(csvPaymentOutput,"%ld-",hop->pathHop->channel);
      }
    }
    fprintf(csvPaymentOutput,",");


    if(arrayLen(payment->ignoredPeers)==0)
      fprintf(csvPaymentOutput, "-1");
    else {
      for(j=0; j<arrayLen(payment->ignoredPeers); j++) {
        ignored = arrayGet(payment->ignoredPeers, j);
        if(j==arrayLen(payment->ignoredPeers)-1)
          fprintf(csvPaymentOutput,"%ld",*ignored);
        else
          fprintf(csvPaymentOutput,"%ld-",*ignored);
      }
    }
    fprintf(csvPaymentOutput,",");

    if(arrayLen(payment->ignoredChannels)==0)
      fprintf(csvPaymentOutput, "-1");
    else {
      for(j=0; j<arrayLen(payment->ignoredChannels); j++) {
        ignored = arrayGet(payment->ignoredChannels, j);
        if(j==arrayLen(payment->ignoredChannels)-1)
          fprintf(csvPaymentOutput,"%ld",*ignored);
        else
          fprintf(csvPaymentOutput,"%ld-",*ignored);
      }
    }
    fprintf(csvPaymentOutput,"\n");
  }

  fclose(csvPaymentOutput);

}

void readPreInputAndInitialize() {
  long nPayments, nPeers, nChannels;
  double paymentMean, pUncoopBefore, pUncoopAfter, RWithholding, gini;
  struct json_object* jpreinput, *jobj;
  unsigned int isPreproc=1;

  jpreinput = json_object_from_file("preinput.json");

  jobj = json_object_object_get(jpreinput, "PaymentMean");
  paymentMean = json_object_get_double(jobj);
  jobj = json_object_object_get(jpreinput, "NPayments");
  nPayments = json_object_get_int64(jobj);
  jobj = json_object_object_get(jpreinput, "NPeers");
  nPeers = json_object_get_int64(jobj);
  jobj = json_object_object_get(jpreinput, "NChannels");
  nChannels = json_object_get_int64(jobj);
  jobj = json_object_object_get(jpreinput, "PUncooperativeBeforeLock");
  pUncoopBefore = json_object_get_double(jobj);
  jobj = json_object_object_get(jpreinput, "PUncooperativeAfterLock");
  pUncoopAfter = json_object_get_double(jobj);
  jobj = json_object_object_get(jpreinput, "PercentageRWithholding");
  RWithholding = json_object_get_double(jobj);
  jobj = json_object_object_get(jpreinput, "Gini");
  gini = json_object_get_double(jobj);

  initializeProtocolData(nPeers, nChannels, pUncoopBefore, pUncoopAfter, RWithholding, gini, isPreproc);
  initializeSimulatorData(nPayments, paymentMean, isPreproc);

  statsInitialize();

  floydWarshall();

}





int main() {
  Event* event;
  //  Peer* peer;
  //ChannelInfo* channelInfo;
  //Channel* channel;
  //Payment* payment;

  printf("main\n");


  readPreInputAndInitialize();

  /*
  payment = hashTableGet(payments, 7);
  if(payment==NULL) printf("NULL\n");
  printf("Payment %ld,%ld,%ld,%ld\n", payment->ID, payment->sender, payment->receiver, payment->amount);

  peer = hashTableGet(peers, 0);
  printf("Peer %ld %d\n", peer->ID, peer->withholdsR);

  channelInfo = hashTableGet(channelInfos, 10);
  printf("ChannelInfo %ld %ld %ld %ld %ld %ld %d\n", channelInfo->ID, channelInfo->channelDirection1, channelInfo->channelDirection2, channelInfo->peer1, channelInfo->peer2, channelInfo->capacity, channelInfo->latency);


  channel = hashTableGet(channels, channelIndex-1);
  printf("Channel %ld %ld %ld %ld %ld %d %d %d\n", channel->ID, channel->channelInfoID, channel->otherChannelDirectionID, channel->counterparty, channel->balance, channel->policy.feeBase, channel->policy.feeProportional, channel->policy.timelock);
  */

  //  csvWriteInput();
  //jsonWriteInput();

  while(heapLen(events) != 0 ) {
    event = heapPop(events, compareEvent);
    simulatorTime = event->time;
    switch(event->type){
    case FINDROUTE:
      findRoute(event);
      break;
    case SENDPAYMENT:
      sendPayment(event);
      break;
    case FORWARDPAYMENT:
      forwardPayment(event);
      break;
    case RECEIVEPAYMENT:
      receivePayment(event);
      break;
    case FORWARDSUCCESS:
      forwardSuccess(event);
      break;
    case RECEIVESUCCESS:
      receiveSuccess(event);
      break;
    case FORWARDFAIL:
      forwardFail(event);
      break;
    case RECEIVEFAIL:
      receiveFail(event);
      break;
    default:
      printf("ERROR wrong event type\n");
    }
  }


  //printPayments();

  jsonWriteOutput();

  csvWriteOutput();

  return 0;
}


/*

// test json writer
int main() {
  long i, nP, nC;
  Peer* peer;
  long sender, receiver;
  Payment *payment;
  Event *event;
  double amount;
  Channel* channel;


  nP = 5;
  nC = 2;

  initializeSimulatorData();
  initializeProtocolData(nP, nC);
  statsInitialize();

  for(i=0; i<nPeers; i++) {
    peer = createPeer(peerIndex,5);
    hashTablePut(peers, peer->ID, peer);
  }


  for(i=1; i<4; i++) {
    connectPeers(i-1, i);
  }
  connectPeers(1, 4);

  jsonWriteInput();

  sender = 0;
  receiver = 3;
  amount = 0.1;
  payment = createPayment(paymentIndex, sender, receiver, amount);
  hashTablePut(payments, payment->ID, payment);
  event = createEvent(eventIndex, 0.0, FINDROUTE, sender, payment->ID);
  events = heapInsert(events, event, compareEvent);

  sender = 0;
  receiver = 3;
  amount = 0.1;
  payment = createPayment(paymentIndex, sender, receiver, amount);
  hashTablePut(payments, payment->ID, payment);
  event = createEvent(eventIndex, 0.0, FINDROUTE, sender, payment->ID);
  events = heapInsert(events, event, compareEvent);

  //this payment fails for peer 2 non cooperative
  sender = 0;
  receiver = 3;
  amount = 0.1;
  payment = createPayment(paymentIndex, sender, receiver, amount);
  hashTablePut(payments, payment->ID, payment);
  event = createEvent(eventIndex, 0.0, FINDROUTE, sender, payment->ID);
  events = heapInsert(events, event, compareEvent);

  //this payment fails for peer 1 non cooperative
  sender = 0;
  receiver = 4;
  amount = 0.1;
  payment = createPayment(paymentIndex, sender, receiver, amount);
  hashTablePut(payments, payment->ID, payment);
  event = createEvent(eventIndex, 0.0, FINDROUTE, sender, payment->ID);
  events = heapInsert(events, event, compareEvent);


 while(heapLen(events) != 0 ) {
    event = heapPop(events, compareEvent);
    simulatorTime = event->time;

    switch(event->type){
    case FINDROUTE:
      findRoute(event);
      break;
    case SENDPAYMENT:
      sendPayment(event);
      break;
    case FORWARDPAYMENT:
      forwardPayment(event);
      break;
    case RECEIVEPAYMENT:
      receivePayment(event);
      break;
    case FORWARDSUCCESS:
      forwardSuccess(event);
      break;
    case RECEIVESUCCESS:
      receiveSuccess(event);
      break;
    case FORWARDFAIL:
      forwardFail(event);
      break;
    case RECEIVEFAIL:
      receiveFail(event);
      break;
    default:
      printf("Error wrong event type\n");
    }
  }


  printPayments();

  jsonWriteOutput();

  return 0;

}

*/


/*
//test statsUpdateLockedFundCost 
int main() {
  long i, nP, nC;
  Peer* peer;
  long sender, receiver;
  Payment *payment;
  Event *event;
  double amount;
  Channel* channel;


  nP = 4;
  nC = 2;

  initializeSimulatorData();
  initializeProtocolData(nP, nC);
  statsInitialize();

  for(i=0; i<nPeers; i++) {
    peer = createPeer(peerIndex,5);
    hashTablePut(peers, peer->ID, peer);
  }


  for(i=1; i<4; i++) {
    connectPeers(i-1, i);
  }


  // failed payment for peer 2 non cooperative in forwardPayment
  sender = 0;
  receiver = 3;
  amount = 0.1;
  payment = createPayment(paymentIndex, sender, receiver, amount);
  hashTablePut(payments, payment->ID, payment);
  event = createEvent(eventIndex, 0.0, FINDROUTE, sender, payment->ID);
  events = heapInsert(events, event, compareEvent);


 while(heapLen(events) != 0 ) {
    event = heapPop(events, compareEvent);
    simulatorTime = event->time;

    switch(event->type){
    case FINDROUTE:
      findRoute(event);
      break;
    case SENDPAYMENT:
      sendPayment(event);
      break;
    case FORWARDPAYMENT:
      forwardPayment(event);
      break;
    case RECEIVEPAYMENT:
      receivePayment(event);
      break;
    case FORWARDSUCCESS:
      forwardSuccess(event);
      break;
    case RECEIVESUCCESS:
      receiveSuccess(event);
      break;
    case FORWARDFAIL:
      forwardFail(event);
      break;
    case RECEIVEFAIL:
      receiveFail(event);
      break;
    default:
      printf("Error wrong event type\n");
    }
  }


  printPayments();
  printStats();


  return 0;

}
*/

/*
//test statsComputePaymentTime 
int main() {
  long i, nP, nC;
  Peer* peer;
  long sender, receiver;
  Payment *payment;
  Event *event;
  double amount;
  Channel* channel;

  
  nP = 4;
  nC = 2;

  initializeSimulatorData();
  initializeProtocolData(nP, nC);
  statsInitialize();

  for(i=0; i<nPeers; i++) {
    peer = createPeer(peerIndex,5);
    hashTablePut(peers, peer->ID, peer);
  }


  for(i=1; i<4; i++) {
    connectPeers(i-1, i);
  }


  //succeed payment
  sender = 0;
  receiver = 3;
  amount = 0.1;
  payment = createPayment(paymentIndex, sender, receiver, amount);
  hashTablePut(payments, payment->ID, payment);
  event = createEvent(eventIndex, 0.0, FINDROUTE, sender, payment->ID);
  events = heapInsert(events, event, compareEvent);

  // failed payment for peer 2 non cooperative in forwardPayment
  sender = 0;
  receiver = 3;
  amount = 0.1;
  payment = createPayment(paymentIndex, sender, receiver, amount);
  hashTablePut(payments, payment->ID, payment);
  event = createEvent(eventIndex, 0.0, FINDROUTE, sender, payment->ID);
  events = heapInsert(events, event, compareEvent);


 while(heapLen(events) != 0 ) {
    event = heapPop(events, compareEvent);
    simulatorTime = event->time;

    switch(event->type){
    case FINDROUTE:
      findRoute(event);
      break;
    case SENDPAYMENT:
      sendPayment(event);
      break;
    case FORWARDPAYMENT:
      forwardPayment(event);
      break;
    case RECEIVEPAYMENT:
      receivePayment(event);
      break;
    case FORWARDSUCCESS:
      forwardSuccess(event);
      break;
    case RECEIVESUCCESS:
      receiveSuccess(event);
      break;
    case FORWARDFAIL:
      forwardFail(event);
      break;
    case RECEIVEFAIL:
      receiveFail(event);
      break;
    default:
      printf("Error wrong event type\n");
    }
  }


  printPayments();
  printStats();


  return 0;

}
*/
/*

//test statsUpdatePayments 
int main() {
  long i, nP, nC;
  Peer* peer;
  long sender, receiver;
  Payment *payment;
  Event *event;
  double amount;
  Channel* channel;

  
  nP = 7;
  nC = 2;

  initializeSimulatorData();
  initializeProtocolData(nP, nC);
  statsInitialize();

  for(i=0; i<nPeers; i++) {
    peer = createPeer(peerIndex,5);
    hashTablePut(peers, peer->ID, peer);
  }


  for(i=1; i<4; i++) {
    connectPeers(i-1, i);
  }

  connectPeers(0, 4);
  connectPeers(4, 5);

  //succeed payment
  sender = 0;
  receiver = 5;
  amount = 0.1;
  payment = createPayment(paymentIndex, sender, receiver, amount);
  hashTablePut(payments, payment->ID, payment);
  event = createEvent(eventIndex, 0.0, FINDROUTE, sender, payment->ID);
  events = heapInsert(events, event, compareEvent);

  // failed payment for no path
  sender = 0;
  receiver = 6;
  amount = 0.1;
  payment = createPayment(paymentIndex, sender, receiver, amount);
  hashTablePut(payments, payment->ID, payment);
  event = createEvent(eventIndex, 0.0, FINDROUTE, sender, payment->ID);
  events = heapInsert(events, event, compareEvent);

  // succeeded payment but uncoop node in forwardsuccess (uncoop if paymentID==2 && peerID==1) 
  sender = 0;
  receiver = 3;
  amount = 0.1;
  payment = createPayment(paymentIndex, sender, receiver, amount);
  hashTablePut(payments, payment->ID, payment);
  event = createEvent(eventIndex, 0.0, FINDROUTE, sender, payment->ID);
  events = heapInsert(events, event, compareEvent);

  //failed payment due to uncoop node in forwardpayment (uncoop if paymentID==3 and peerID==2)
  sender = 0;
  receiver = 3;
  amount = 0.1;
  payment = createPayment(paymentIndex, sender, receiver, amount);
  hashTablePut(payments, payment->ID, payment);
  event = createEvent(eventIndex, 0.0, FINDROUTE, sender, payment->ID);
  events = heapInsert(events, event, compareEvent);


 while(heapLen(events) != 0 ) {
    event = heapPop(events, compareEvent);
    simulatorTime = event->time;

    switch(event->type){
    case FINDROUTE:
      findRoute(event);
      break;
    case SENDPAYMENT:
      sendPayment(event);
      break;
    case FORWARDPAYMENT:
      forwardPayment(event);
      break;
    case RECEIVEPAYMENT:
      receivePayment(event);
      break;
    case FORWARDSUCCESS:
      forwardSuccess(event);
      break;
    case RECEIVESUCCESS:
      receiveSuccess(event);
      break;
    case FORWARDFAIL:
      forwardFail(event);
      break;
    case RECEIVEFAIL:
      receiveFail(event);
      break;
    default:
      printf("Error wrong event type\n");
    }
  }


  printPayments();
  printStats();


  return 0;
}
*/

/*
//test channels not present 
int main() {
  long i, nP, nC;
  Peer* peer;
  long sender, receiver;
  Payment *payment;
  Event *event;
  double amount;
  Channel* channel;

  
  nP = 5;
  nC = 2;

  initializeSimulatorData();
  initializeProtocolData(nP, nC);

  for(i=0; i<nPeers; i++) {
    peer = createPeer(peerIndex,5);
    hashTablePut(peers, peer->ID, peer);
  }


  for(i=1; i<4; i++) {
    connectPeers(i-1, i);
  }

  connectPeers(1, 4);


  //test is!Present in forwardSuccess
  connectPeers(1, 4);

  sender = 0;
  receiver = 4;
  amount = 0.1;
  simulatorTime = 0.0;
  payment = createPayment(paymentIndex, sender, receiver, amount);
  hashTablePut(payments, payment->ID, payment);
  event = createEvent(eventIndex, simulatorTime, FINDROUTE, sender, payment->ID);
  events = heapInsert(events, event, compareEvent);

  //for this payment only peer 1 must be not cooperative
  sender = 0;
  receiver = 3;
  amount = 0.1;
  simulatorTime = 0.05;
  payment = createPayment(paymentIndex, sender, receiver, amount);
  hashTablePut(payments, payment->ID, payment);
  event = createEvent(eventIndex, simulatorTime, FINDROUTE, sender, payment->ID);
  events = heapInsert(events, event, compareEvent);
  // end test is!Present in forwardSuccess



  //test is!Present in forwardFail
  connectPeers(1, 4);

  channel = hashTableGet(channels, 6);
  channel->balance = 0.0;

  sender = 0;
  receiver = 4;
  amount = 0.1;
  simulatorTime = 0.0;
  payment = createPayment(paymentIndex, sender, receiver, amount);
  hashTablePut(payments, payment->ID, payment);
  event = createEvent(eventIndex, simulatorTime, FINDROUTE, sender, payment->ID);
  events = heapInsert(events, event, compareEvent);

  //for this payment only peer 1 must be not cooperative
  sender = 0;
  receiver = 3;
  amount = 0.1;
  simulatorTime = 0.05;
  payment = createPayment(paymentIndex, sender, receiver, amount);
  hashTablePut(payments, payment->ID, payment);
  event = createEvent(eventIndex, simulatorTime, FINDROUTE, sender, payment->ID);
  events = heapInsert(events, event, compareEvent);
  // end test is!Present in forwardFail




  //test is!Present in forwardPayment

  //for this payment peer 2 must be non-cooperative
  sender = 0;
  receiver = 3;
  amount = 0.1;
  simulatorTime = 0.0;
  payment = createPayment(paymentIndex, sender, receiver, amount);
  hashTablePut(payments, payment->ID, payment);
  event = createEvent(eventIndex, 0.0, FINDROUTE, sender, payment->ID);
  events = heapInsert(events, event, compareEvent);

  
  sender = 0;
  receiver = 3;
  amount = 0.1;
  //  simulatorTime = 0.2;
  payment = createPayment(paymentIndex, sender, receiver, amount);
  hashTablePut(payments, payment->ID, payment);
  event = createEvent(eventIndex, 0.1, FINDROUTE, sender, payment->ID);
  events = heapInsert(events, event, compareEvent);
  // end test is!Present in forwardPayment


  
  //test is!Present in receiveFail

  //for this payment peer 3 must be non-cooperative
  sender = 0;
  receiver = 3;
  amount = 0.1;
  simulatorTime = 0.0;
  payment = createPayment(paymentIndex, sender, receiver, amount);
  hashTablePut(payments, payment->ID, payment);
  event = createEvent(eventIndex, 0.0, FINDROUTE, sender, payment->ID);
  events = heapInsert(events, event, compareEvent);

  
  sender = 0;
  receiver = 3;
  amount = 0.1;
  //  simulatorTime = 0.2;
  payment = createPayment(paymentIndex, sender, receiver, amount);
  hashTablePut(payments, payment->ID, payment);
  event = createEvent(eventIndex, 0.2, FINDROUTE, sender, payment->ID);
  events = heapInsert(events, event, compareEvent);
  // end test is!Present in receiveFail
  


 while(heapLen(events) != 0 ) {
    event = heapPop(events, compareEvent);

    switch(event->type){
    case FINDROUTE:
      findRoute(event);
      break;
    case SENDPAYMENT:
      sendPayment(event);
      break;
    case FORWARDPAYMENT:
      forwardPayment(event);
      break;
    case RECEIVEPAYMENT:
      receivePayment(event);
      break;
    case FORWARDSUCCESS:
      forwardSuccess(event);
      break;
    case RECEIVESUCCESS:
      receiveSuccess(event);
      break;
    case FORWARDFAIL:
      forwardFail(event);
      break;
    case RECEIVEFAIL:
      receiveFail(event);
      break;
    default:
      printf("Error wrong event type\n");
    }
  }


  printPayments();


  return 0;
}
*/

/*
//test peer not cooperatives
int main() {
  long i, nP, nC;
  Peer* peer;
  long sender, receiver;
  Payment *payment;
  Event *event;
  double amount;
  Channel* channel;

  //test peer 2 not cooperative before/after lock
  nP = 6;
  nC = 2;

  initializeSimulatorData();
  initializeProtocolData(nP, nC);

  for(i=0; i<nPeers; i++) {
    peer = createPeer(peerIndex,5);
    hashTablePut(peers, peer->ID, peer);
  }


  for(i=1; i<5; i++) {
    connectPeers(i-1, i);
  }

 
  connectPeers(1, 5);
  connectPeers(5, 3);
  channel = hashTableGet(channels, 8);
  channel->policy.timelock = 5;
  

  sender = 0;
  receiver = 4;
  amount = 1.0;
  simulatorTime = 0.0;
  payment = createPayment(paymentIndex, sender, receiver, amount);
  hashTablePut(payments, payment->ID, payment);
  event = createEvent(eventIndex, simulatorTime, FINDROUTE, sender, payment->ID);
  events = heapInsert(events, event, compareEvent);



 while(heapLen(events) != 0 ) {
    event = heapPop(events, compareEvent);

    switch(event->type){
    case FINDROUTE:
      findRoute(event);
      break;
    case SENDPAYMENT:
      sendPayment(event);
      break;
    case FORWARDPAYMENT:
      forwardPayment(event);
      break;
    case RECEIVEPAYMENT:
      receivePayment(event);
      break;
    case FORWARDSUCCESS:
      forwardSuccess(event);
      break;
    case RECEIVESUCCESS:
      receiveSuccess(event);
      break;
    case FORWARDFAIL:
      forwardFail(event);
      break;
    case RECEIVEFAIL:
      receiveFail(event);
      break;
    default:
      printf("Error wrong event type\n");
    }
  }


  printBalances();


  return 0;
}
*/
/*
//test payments

int main() {
  long i, nP, nC;
  Peer* peer;
  long sender, receiver;
  Payment *payment;
  Event *event;
  double amount;
  Channel* channel;


  nP = 5;
  nC = 2;

  initializeSimulatorData();
  initializeProtocolData(nP, nC);

  for(i=0; i<nPeers; i++) {
    peer = createPeer(peerIndex,5);
    hashTablePut(peers, peer->ID, peer);
  }


  for(i=1; i<5; i++) {
    connectPeers(i-1, i);
  }

  

   
  //test fail
  channel = hashTableGet(channels, 6);
  channel->balance = 0.5;
  //end test fail
  

  
  //test ignoredChannels hop
  channel = hashTableGet(channels, 6);
  channel->balance = 0.5;
  connectPeers(3,4);
  channel = hashTableGet(channels, 8);
  channel->policy.timelock = 5;
  //end test ignoredChannels hop
  

  
  //test ignoredChannels sender
  channel = hashTableGet(channels, 0);
  channel->balance = 0.5;
  connectPeers(0,1);
  channel = hashTableGet(channels, 8);
  channel->policy.timelock = 5;
  //end test ignoredChannels sender
  

  //test full payment
  sender = 0;
  receiver = 4;
  amount = 1.0;
  simulatorTime = 0.0;
  payment = createPayment(paymentIndex, sender, receiver, amount);
  hashTablePut(payments, payment->ID, payment);
  event = createEvent(eventIndex, simulatorTime, FINDROUTE, sender, payment->ID);
  events = heapInsert(events, event, compareEvent);
  //end test full payment


  
  //test two payments: success and fail
  sender = 0;
  receiver = 4;
  amount = 1.0;
  simulatorTime = 0.0;
  payment = createPayment(paymentIndex, sender, receiver, amount);
  hashTablePut(payments, payment->ID, payment);
  event = createEvent(eventIndex, simulatorTime, FINDROUTE, sender, payment->ID);
  events = heapInsert(events, event, compareEvent);

  sender = 4;
  receiver = 0;
  amount = 4.0;
  simulatorTime = 0.0;
  payment = createPayment(paymentIndex, sender, receiver, amount);
  hashTablePut(payments, payment->ID, payment);
  event = createEvent(eventIndex, simulatorTime, FINDROUTE, sender, payment->ID);
  events = heapInsert(events, event, compareEvent);
  //end test two payments
  

  
  //test payment without hops
  sender = 0;
  receiver = 1;
  amount = 1.0;
  simulatorTime = 0.0;
  payment = createPayment(paymentIndex, sender, receiver, amount);
  hashTablePut(payments, payment->ID, payment);
  event = createEvent(eventIndex, simulatorTime, FINDROUTE, sender, payment->ID);
  events = heapInsert(events, event, compareEvent);
  //end test payment without hops
  

  
  //test more payments from same source
  sender = 0;
  receiver = 4;
  amount = 0.3;
  simulatorTime = 0.0;
  payment = createPayment(paymentIndex, sender, receiver, amount);
  hashTablePut(payments, payment->ID, payment);
  event = createEvent(eventIndex, simulatorTime, FINDROUTE, sender, payment->ID);
  events = heapInsert(events, event, compareEvent);
  
  sender = 0;
  receiver = 4;
  amount = 0.3;
  simulatorTime = 0.0;
  payment = createPayment(paymentIndex, sender, receiver, amount);
  hashTablePut(payments, payment->ID, payment);
  event = createEvent(eventIndex, simulatorTime, FINDROUTE, sender, payment->ID);
  events = heapInsert(events, event, compareEvent);

  sender = 0;
  receiver = 4;
  amount = 0.3;
  simulatorTime = 0.0;
  payment = createPayment(paymentIndex, sender, receiver, amount);
  hashTablePut(payments, payment->ID, payment);
  event = createEvent(eventIndex, simulatorTime, FINDROUTE, sender, payment->ID);
  events = heapInsert(events, event, compareEvent);
   //end test more payments from same source
   

  


 while(heapLen(events) != 0 ) {
    event = heapPop(events, compareEvent);

    switch(event->type){
    case FINDROUTE:
      findRoute(event);
      break;
    case SENDPAYMENT:
      sendPayment(event);
      break;
    case FORWARDPAYMENT:
      forwardPayment(event);
      break;
    case RECEIVEPAYMENT:
      receivePayment(event);
      break;
    case FORWARDSUCCESS:
      forwardSuccess(event);
      break;
    case RECEIVESUCCESS:
      receiveSuccess(event);
      break;
    case FORWARDFAIL:
      forwardFail(event);
      break;
    case RECEIVEFAIL:
      receiveFail(event);
      break;
    default:
      printf("Error wrong event type\n");
    }
  }

  //TODO: stampare ordinatamente le balances per testare correttezza
  printBalances();


  return 0;
}

*/

/*HashTable* peers, *channels, *channelInfos;
long nPeers, nChannels;
 
//test trasformPathIntoRoute
int main() {
  PathHop* pathHop;
  Array *ignored;
  long i, sender, receiver;
  long fakeIgnored = -1;
  Route* route;
  Array* routeHops, *pathHops;
  RouteHop* routeHop;
  Peer* peer;
  Channel* channel;
  ChannelInfo * channelInfo;

  ignored = arrayInitialize(1);
  ignored = arrayInsert(ignored, &fakeIgnored);

  peers = hashTableInitialize(2);
  channels = hashTableInitialize(2);
  channelInfos= hashTableInitialize(2);

  nPeers=5;

  for(i=0; i<nPeers; i++) {
      peer = createPeer(5);
      hashTablePut(peers, peer->ID, peer);
    }

    for(i=1; i<5; i++) {
      connectPeers(i-1, i);
    }

    pathHops=dijkstra(0, 4, 1.0, ignored, ignored );
    route = transformPathIntoRoute(pathHops, 1.0, 5);
    printf("Route/n");
    if(route==NULL) {
      printf("Null route/n");
      return 0;
    }

  for(i=0; i < arrayLen(route->routeHops); i++) {
    routeHop = arrayGet(route->routeHops, i);
    pathHop = routeHop->pathHop;
    printf("HOP %ld\n", i);
    printf("(Sender, Receiver, Channel) = (%ld, %ld, %ld)\n", pathHop->sender, pathHop->receiver, pathHop->channel);
    printf("Amount to forward: %lf\n", routeHop->amountToForward);
    printf("Timelock: %d\n\n", routeHop->timelock);
  }

  printf("Total amount: %lf\n", route->totalAmount);
  printf("Total fee: %lf\n", route->totalFee);
  printf("Total timelock: %d\n", route->totalTimelock);

  return 0;
}
*/
/*
// Test Yen

HashTable* peers, *channels, *channelInfos;
long nPeers, nChannels;

int main() {
  Array *paths;
  Array* path;
  PathHop* hop;
  long i, j;
  Peer* peer;
  Channel* channel;
  ChannelInfo * channelInfo;

  peers = hashTableInitialize(2);
  channels = hashTableInitialize(2);
  channelInfos= hashTableInitialize(2);

  nPeers=8;

  

  for(i=0; i<nPeers; i++) {
    peer = createPeer(5);
    hashTablePut(peers, peer->ID, peer);
  }

  Policy policy;
  policy.fee=0.0;
  policy.timelock=1.0;

  for(i=1; i<5; i++) {
    connectPeers(i-1, i);
  }

  connectPeers(0, 5);
  connectPeers(5,6);
  connectPeers(6,4);

  connectPeers(0,7);
  connectPeers(7,4);




  long *currChannelID;
  for(i=0; i<nPeers; i++) {
    peer = hashTableGet(peers, i);
    //    printf("%ld ", arrayLen(peer->channel));
    for(j=0; j<arrayLen(peer->channel); j++) {
      currChannelID=arrayGet(peer->channel, j);
      if(currChannelID==NULL) {
        printf("null\n");
        continue;
      }
      channel = hashTableGet(channels, *currChannelID);
      printf("Peer %ld is connected to peer %ld through channel %ld\n", i, channel->counterparty, channel->ID);
      }
  }



  printf("\nYen\n");
  paths=findPaths(0, 4, 0.0);
  printf("%ld\n", arrayLen(paths));
  for(i=0; i<arrayLen(paths); i++) {
    printf("\n");
     path = arrayGet(paths, i);
     for(j=0;j<arrayLen(path); j++) {
       hop = arrayGet(path, j);
       printf("(Sender, Receiver, Channel) = (%ld, %ld, %ld) ", hop->sender, hop->receiver, hop->channel);
     }
  }

  return 0;
}
*/

/*
//test dijkstra
int main() {
  Array *hops;
  PathHop* hop;
  Array *ignored;
  long i, sender, receiver;
  long fakeIgnored = -1;

  ignored = arrayInitialize(1);
  ignored = arrayInsert(ignored, &fakeIgnored);

  initialize();
  printf("\nDijkstra\n");

  sender = 4;
  receiver = 3;
  hops=dijkstra(sender, receiver, 0.0, ignored, ignored );
  printf("From node %ld to node %ld\n", sender, receiver);
  for(i=0; i<arrayLen(hops); i++) {
    hop = arrayGet(hops, i);
    printf("(Sender, Receiver, Channel) = (%ld, %ld, %ld) ", hop->sender, hop->receiver, hop->channel);
  }
  printf("\n\n");

  sender = 0;
  receiver = 1;
  hops=dijkstra(sender, receiver, 0.0, ignored, ignored );
  printf("From node %ld to node %ld\n", sender, receiver);
  for(i=0; i<arrayLen(hops); i++) {
    hop = arrayGet(hops, i);
    printf("(Sender, Receiver, Channel) = (%ld, %ld, %ld) ", hop->sender, hop->receiver, hop->channel);
  }
  printf("\n");

  sender = 0;
  receiver = 0;
  printf("From node %ld to node %ld\n", sender, receiver);
  hops=dijkstra(sender,receiver, 0.0, ignored, ignored );
  if(hops != NULL) {
    for(i=0; i<arrayLen(hops); i++) {
      hop = arrayGet(hops, i);
      printf("(Sender, Receiver, Channel) = (%ld, %ld, %ld) ", hop->sender, hop->receiver, hop->channel);
    }
  }
  printf("\n");


  return 0;
}
*/

/*
int main() {
  Array *a;
  long i, *data;

  a=arrayInitialize(10);

  for(i=0; i< 21; i++){
    data = GC_MALLOC(sizeof(long));
    *data = i;
   a = arrayInsert(a,data);
  }

  for(i=0; i<21; i++) {
    data = arrayGet(a,i);
    if(data!=NULL) printf("%ld ", *data);
    else printf("null ");
  }

  printf("\n%ld ", a->size);

  return 0;
}*/

/*
int main() {
  HashTable *ht;
  long i;
  Event *e;
  long N=100000;

  ht = initializeHashTable(10);

  for(i=0; i<N; i++) {
    e = GC_MALLOC(sizeof(Event));
    e->time=0.0;
    e->ID=i;
    strcpy(e->type, "Send");

    put(ht, e->ID, e);

}

  long listDim[10]={0};
  Element* el;
  for(i=0; i<10;i++) {
    el =ht->table[i];
    while(el!=NULL){
      listDim[i]++;
      el = el->next;
    }
  }

  for(i=0; i<10; i++)
   printf("%ld ", listDim[i]);

  printf("\n");

  return 0;
}


/*
int main() {

  const gsl_rng_type * T;
  gsl_rng * r;

  int i, n = 10;
  double mu = 0.1;


  gsl_rng_env_setup();

  T = gsl_rng_default;
  r = gsl_rng_alloc (T);


  for (i = 0; i < n; i++)
    {
      double  k = gsl_ran_exponential (r, mu);
      printf (" %lf", k);
    }

  printf ("\n");
  gsl_rng_free (r);

  return 0;
   }
*/

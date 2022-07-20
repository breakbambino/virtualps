
#ifndef __INTERESTDELEGATOR_H__
#define __INTERESTDELEGATOR_H__

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/util/scheduler.hpp>

#include <boost/asio/io_service.hpp>
#include <iostream>

using namespace ndn;
using namespace ndn::util;

using InterestPtr = std::shared_ptr<Interest>;
using FacePtr = std::shared_ptr<Face>;
using SchedulerPtr = std::shared_ptr<Scheduler>;

class InterestDelegator {
public:
	FacePtr m_face;
	SchedulerPtr m_scheduler;
	InterestPtr m_interest;
	scheduler::EventId m_eventId;

public:
	std::function<void(const Data &)> dataHandler;
	std::function<void(const Interest &, const lp::Nack &)> nackHandler;
	std::function<void(const Interest &)> timeoutHandler;

public:
	InterestDelegator(FacePtr face, SchedulerPtr scheduler)
		: m_face(face)
		, m_scheduler(scheduler)
		, m_interest(nullptr)
	{
	}

	InterestDelegator(FacePtr face, SchedulerPtr scheduler, InterestPtr interest, bool express = true)
		: m_face(face)
		, m_scheduler(scheduler)
		, m_interest(interest)
	{
		if(express) {
			delayedInterest(m_interest);
		}
	}

	virtual ~InterestDelegator() {
	}

	void cancel() {

		if((bool)m_eventId) {
			m_eventId.cancel();
			m_eventId.reset();
		}
	}

	void setHandler(std::function<void(const Data&)> dataHandler,
			std::function<void(const Interest&, const lp::Nack&)> nackHandler,
			std::function<void(const Interest&)> timeoutHandler) {

	}

	void send() {
		if(m_interest == nullptr) {
			return;
		}

		delayedInterest(m_interest);
	}

	void send(Interest interest) {
		m_interest = std::make_shared<Interest>(interest);
		delayedInterest(m_interest);
	}

	void send(InterestPtr interest) {
		m_interest = interest;
		delayedInterest(m_interest);
	}

private:
	void onData(const Interest &interest, const Data &data)  {
		//std::cout << "Received Data " << data << std::endl;
		//onSignalData(data);
		dataHandler(data);
	}

	void onNack(const Interest &interest, const lp::Nack &nack)  {
		//std::cout << "Received Nack with reason " << nack.getReason() << " for " << interest << std::endl;
		nackHandler(interest, nack);
	}

	void onTimeout(const Interest &interest)  {
		//std::cout << "Timeout for " << interest << std::endl;
		timeoutHandler(interest);

		// 동일한 interest를 보내면 중복 오류 발생함
		// 새로 생성하여 전송해야 함
		InterestPtr one = std::make_shared<Interest>(interest.getName());
		if(interest.hasApplicationParameters()) {
			one->setApplicationParameters(interest.getApplicationParameters());
		}
		one->setCanBePrefix(interest.getCanBePrefix());
		one->setForwardingHint(interest.getForwardingHint());
		one->setHopLimit(interest.getHopLimit());
		one->setInterestLifetime(interest.getInterestLifetime());
		one->setMustBeFresh(interest.getMustBeFresh());
#if 0
		m_scheduler->schedule(0_ms, [this, one] {
			delayedInterest(one);
		});
#else
		delayedInterest(one);
#endif
	}

	void delayedInterest(InterestPtr interest) {
		//std::cout << "Sending Interest " << *interest << std::endl;

		PendingInterestHandle handle;
		handle = m_face->expressInterest(*interest,
				bind(&InterestDelegator::onData, this, _1, _2),
				bind(&InterestDelegator::onNack, this, _1, _2),
				bind(&InterestDelegator::onTimeout, this, _1));
	}
};

#endif


#ifndef MSExtendedTrafficLightLogic_h
#define MSExtendedTrafficLightLogic_h

#include "MSSimpleTrafficLightLogic.h"

class MSExtendedTrafficLightLogic
	: public MSSimpleTrafficLightLogic
{
public:
	MSExtendedTrafficLightLogic(const std::string &id, const Phases &phases,
        size_t step, size_t delay)
		: MSSimpleTrafficLightLogic(id, phases, step, delay) { }
	~MSExtendedTrafficLightLogic() { }
	virtual void init(const std::vector<MSLane*> &lanes,
        std::map<std::string, std::vector<std::string> > &edgeContinuations) = 0;


};

#endif

#include "Engine.h"

namespace Shipping {

Segment::Segment(Fwk::String name, Mode mode, Network *network):
	Fwk::NamedInterface(name),
	network_(network),
	mode_(mode),
	exp_support_(Segment::expediteNotSupported())
	{
		SegmentReactor::Ptr reactor = SegmentReactor::SegmentReactorNew(this, network_);
		segmentReactorIs(reactor);
	}

string
Segment::modeName(Mode m)
{
	switch(m) {
		case truck_: return "truck";
		case boat_: return "boat";
		case plane_: return "plane";
		default: return "";
	}
}

void
Segment::sourceIs(Fwk::Ptr<Location> location) {
	if(location && source_ && location->name() == source_->name()) return;

	if (!location ||
		(location->locationType() != Location::terminal()) ||
		(location->locationType() == Location::terminal()
			&& dynamic_cast<Terminal*>(location.ptr())->vehicleType() == mode())) {
	
		if (source_) {
			source_->segmentDel(this->name());
			Ptr nil = Ptr();
			returnSegmentIs(nil);
		}

		source_ = location;
		if (location) {
			location->segmentIs(this);
		}
	}	
}

void
Segment::returnSegmentIs(Ptr &r) {
	if (return_segment_ && r && return_segment_->name() == r->name())
		return;
	if (r && r->mode() != mode())
		return;

	if (return_segment_) {
		return_segment_->return_segment_ = Ptr();
	}
	else if (r && r->return_segment_) {
		r->return_segment_->return_segment_ = Ptr();
	}
	return_segment_ = r;
	if(r) r->return_segment_ = this;
}

void
Segment::NotifieeConst::notifierIs(const Segment::PtrConst& _notifier) {
	Segment::Ptr notifierSave(const_cast<Segment *>(notifier_.ptr()));
	if(_notifier==notifier_) return;
	notifier_ = _notifier;
	if(notifierSave) {
		notifierSave->deleteNotifiee(this);
	}
	if(_notifier) {
		_notifier->newNotifiee(this);
	}
	if(isNonReferencing_) {
		if(notifierSave) notifierSave->newRef();
		if(notifier_) notifier_->deleteRef();
	}
}

Segment::NotifieeConst::~NotifieeConst() {
	if(notifier_) {
		notifier_->deleteNotifiee(this);
	}
	if(notifier_&&isNonReferencing()) notifier_->newRef();
}

void
Segment::arrivingShipmentIs(Fwk::Ptr<Shipment> &shipment)
{
	//TODO
	PackageCount packagesPerVehicle = network_->fleet()->capacity(mode());
	VehicleCount numVehiclesOnSegment = numVehicles();

	PackageCount totalPossiblePackages =
		PackageCount(numVehiclesOnSegment.value() * packagesPerVehicle.value());

	PackageCount spaceAvailable = PackageCount(totalPossiblePackages.value() - segmentLoad().value());

	if (shipment->load() <= spaceAvailable) {
		segmentLoadIs( PackageCount(segmentLoad().value() + shipment->load().value()) );
		numShipmentsReceivedIs( ShipmentCount(numShipmentsReceived().value() + 1) );

		// tell all the notifiees
		if(notifiees()) {
			for(NotifieeIterator n=notifieeIter(); n.ptr(); ++n) {
				try { n->onShipmentArrival(shipment); }
				catch(...) {
					cerr << "Segment::onShipmentArrival() notification for "
					<< shipment->name() << " unsuccessful" << endl;
				}
			}
		}
	}
	else {
		numShipmentsToldToWaitIs( ShipmentCount(numShipmentsToldToWait().value() + 1) );
		// throw an exception
		throw Fwk::RangeException("segment cannot accept shipment"); 
	}
}


void
Segment::SegmentReactor::onShipmentArrival(Fwk::Ptr<Shipment> &shipment)
{
	//TODO
	Activity::Ptr activity = activityManager_->activityNew("ForwardingActivity");
	activity->lastNotifieeIs( new ForwardActivityReactor(activityManager_, activity.ptr(), notifier().ptr(), shipment.ptr()) );
	Segment::Ptr seg = notifier();
	Fleet::PtrConst fleet = network_->fleet();
	Time timeToTraverse = Time(seg->length().value() / fleet->speed(seg->mode()).value());
	shipment->latencyInc(Hours(seg->length().value() / fleet->speed(seg->mode()).value()));
	activity->nextTimeIs(activityManager_->now().value() + timeToTraverse.value());
	activity->statusIs(Activity::nextTimeScheduled);
}

void
Location::arrivingShipmentIs(Fwk::Ptr<Shipment> &shipment)
{
	//TODO
	// tell all the notifiees
	if(notifiees()) {
		for(NotifieeIterator n=notifieeIter(); n.ptr(); ++n) {
			try { n->onShipmentArrival(shipment); }
			catch(...) {
				cerr << "Location::onShipmentArrival() notification for "
				<< shipment->name() << " unsuccessful" << endl;
			}
		}
	}
}


void
Location::NotifieeConst::notifierIs(const Location::PtrConst& _notifier) {
   Location::Ptr notifierSave(const_cast<Location *>(notifier_.ptr()));
   if(_notifier==notifier_) return;
   notifier_ = _notifier;
   if(notifierSave) {
	  notifierSave->deleteNotifiee(this);
   }
   if(_notifier) {
	  _notifier->newNotifiee(this);
   }
   if(isNonReferencing_) {
	  if(notifierSave) notifierSave->newRef();
	  if(notifier_) notifier_->deleteRef();
   }
}

Location::NotifieeConst::~NotifieeConst() {
   if(notifier_) {
	  notifier_->deleteNotifiee(this);
   }
   if(notifier_&&isNonReferencing()) notifier_->newRef();
}

void
Location::LocationReactor::onShipmentArrival(Fwk::Ptr<Shipment> &shipment)
{
	//TODO
	LocationType locationType = notifier()->locationType();
	string destinationName = shipment->dest()->name();
	string sourceName = shipment->source()->name();
	Statistics::Ptr stats = const_cast<Statistics*>(network_->statistics().ptr());

	if (locationType == Location::customer() && sourceName != notifier()->name()) {		
		if (destinationName == notifier()->name()) {
			//at destination
			Customer *customer = dynamic_cast<Customer*>(notifier().ptr());
			customer->shipmentsReceivedIs(ShipmentCount(customer->shipmentsReceived().value() + 1));
			customer->totalLatencyInc(shipment->latency());
			customer->totalCostInc(shipment->path()->cost());
			stats->deliveredShipmentIs(shipment);
		}
		else {
			// DROP SHIPMENT
			stats->droppedShipmentIs(shipment);
		}
	}
	else {
		Segment::Ptr segment;
		try {
			size_t thisLocationIndex = shipment->path()->locationIndex(notifier());
			if (thisLocationIndex + 1 < shipment->path()->numParts()) {
				Segment::PtrConst seg = shipment->path()->part(thisLocationIndex + 1).seg;
				segment = const_cast<Segment*>(seg.ptr());

				// may throw exception
				segment->arrivingShipmentIs(shipment);
			}
			else {
				// DROP SHIPMENT
				stats->droppedShipmentIs(shipment);
				cerr << __FILE__":"<<__LINE__<<": LocationReactor::onShipmentArival() next segment index out of bounds." << endl;
			}
		} catch(...) {
			// create retry activity
			Activity::Ptr activity = activityManager_->activityNew("RetryActivity");
			activity->lastNotifieeIs( new RetryActivityReactor(activityManager_, activity.ptr(), shipment.ptr(), segment.ptr(), network_) );
			double wait = dynamic_cast<RetryActivityReactor*>(activity->notifiee().ptr())->wait();
			activity->nextTimeIs(activityManager_->now().value() + wait);
			activity->statusIs(Activity::nextTimeScheduled);
		}
	}
}

void RetryActivityReactor::onStatus() {
	switch (activity_->status()) {
		case Activity::executing:
		{
			//Retry forwarding the shipment
			try {
				segment_->arrivingShipmentIs(shipment_);

				// If we get to this line, we know this was successful
				// otherwise we would throw an exception.
				successfullyForwardedShipment_ = true;
			} catch(...) { /* we will reschedule ourselves */ }
			break;
		}
	
		case Activity::free:
		{
			if (!successfullyForwardedShipment_) {
				if (totalTimeWaiting_ <= MAX_WAIT) {
					activity_->nextTimeIs(Time(activity_->nextTime().value() + wait_));
					totalTimeWaiting_ += wait_;
					wait_ *= WAIT_MULTIPLE;
					activity_->statusIs(Activity::nextTimeScheduled);
				}
				else {
					segment_->numShipmentsRefusedIs( ShipmentCount(segment_->numShipmentsRefused().value() + 1) );
					Statistics::Ptr stats = const_cast<Statistics*>(network_->statistics().ptr());
					stats->droppedShipmentIs(shipment_);
				}
			}
			break;
		}

		case Activity::nextTimeScheduled:
		{
			//add myself to be scheduled
			manager_->lastActivityIs(activity_);
			break;
		}

		default: break;
	}
}

void InjectActivityReactor::onStatus() {
    switch (activity_->status()) {
	    case Activity::executing:
	    {
	    	Shipment::Ptr shipment = network_->shipmentNew(customer_, customer_->destination(), customer_->shipmentSize());
			customer_->arrivingShipmentIs(shipment);
			break;
		}
	    case Activity::free:
	    {
			//when done, automatically enqueue myself for next execution
			activity_->nextTimeIs(Time(activity_->nextTime().value() + rate_));
			activity_->statusIs(Activity::nextTimeScheduled);
			break;
		}
	    case Activity::nextTimeScheduled:
	    {
			//add myself to be scheduled
			manager_->lastActivityIs(activity_);
			break;
		}
	    default: break;
    }
}

void ForwardActivityReactor::onStatus() {
    switch (activity_->status()) {
	    case Activity::executing:
	    {
	    	segment_->segmentLoadIs( PackageCount(segment_->segmentLoad().value() - shipment_->load().value()) );
	    	Location::Ptr next = segment_->returnSegment()->source();
	    	next->arrivingShipmentIs(shipment_);
			break;
		}
	    case Activity::free:
			break;

	    case Activity::nextTimeScheduled:
			//add myself to be scheduled
			manager_->lastActivityIs(activity_);
			break;

	    default: break;
    }
}

void FleetActivityReactor::onStatus() {
    switch (activity_->status()) {
	    case Activity::executing:
	    {
	    	// switch fleet attributes
	    	if (fleet_->timeOfDay() == Fleet::night())
	    		fleet_->timeOfDayIs(Fleet::day());
	    	else fleet_->timeOfDayIs(Fleet::night());
			break;
		}
	    case Activity::free:
	    {
			//when done, automatically enqueue myself for next execution
			activity_->nextTimeIs(Time(activity_->nextTime().value() + HALF_DAY));
			activity_->statusIs(Activity::nextTimeScheduled);
			break;
		}
	    case Activity::nextTimeScheduled:
	    {
			//add myself to be scheduled
			manager_->lastActivityIs(activity_);
			break;
		}
	    default: break;
    }
}


void
Customer::transferRateIs(ShipmentCount tr){
	transfer_rate_ = tr;
	// tell all the notifiees
	if(notifiees()) {
		for(NotifieeIterator n=notifieeIter(); n.ptr(); ++n) {
			try { n->onTransferRate(tr); }
			catch(...) {
				cerr << "Customer::onTransferRate() notification was unsuccessful" << endl;
			}
		}
	}
}

void
Customer::shipmentSizeIs(PackageCount pc){
	shipment_size_ = pc;
	// tell all the notifiees
	if(notifiees()) {
		for(NotifieeIterator n=notifieeIter(); n.ptr(); ++n) {
			try { n->onShipmentSize(pc); }
			catch(...) {
				cerr << "Customer::onShipmentSize() notification was unsuccessful" << endl;
			}
		}
	}
}

void
Customer::destinationIs(Customer::Ptr c){
	destination_ = c;

	// tell all the notifiees
	if(notifiees()) {
		for(NotifieeIterator n=notifieeIter(); n.ptr(); ++n) {
			try { n->onDestination(c); }
			catch(...) {
				cerr << "Customer::onDestination() notification was unsuccessful" << endl;
			}
		}
	}
	else {

		cout << "NO CUSTOMER REACTOR! notifiees()=" << notifiees() << " reactor=" << endl; //customerReactor_ << endl;
	}
}

void
Customer::NotifieeConst::notifierIs(const Customer::PtrConst& _notifier) {
   Customer::Ptr notifierSave(const_cast<Customer *>(notifier_.ptr()));
   if(_notifier==notifier_) return;
   notifier_ = _notifier;
   if(notifierSave) {
	  notifierSave->deleteNotifiee(this);
   }
   if(_notifier) {
	  _notifier->newNotifiee(this);
   }
   if(isNonReferencing_) {
	  if(notifierSave) notifierSave->newRef();
	  if(notifier_) notifier_->deleteRef();
   }
}

Customer::NotifieeConst::~NotifieeConst() {
   if(notifier_) {
	  notifier_->deleteNotifiee(this);
   }
   if(notifier_&&isNonReferencing()) notifier_->newRef();
}

bool
Customer::CustomerReactor::customerIsReady() const
{
//cout << __FILE__ << ":" << __LINE__ << " notifier=" << notifier().ptr() << " " << notifier()->name() <<endl;	
	if (attributesSet_[transferRate_] && notifier()->transferRate().value() > 0 &&
		attributesSet_[shipmentSize_] && notifier()->shipmentSize().value() > 0 &&
		attributesSet_[dest_]) {
//cout << __FILE__ << ":" << __LINE__ << " customerIsReady() returning true"<<endl;			
		return true;
	}
//cout << __FILE__ << ":" << __LINE__ << " "<< notifier()->name() << " not ready " << endl;		
	return false;
}

void
Customer::CustomerReactor::onTransferRate(ShipmentCount transferRate)
{
	//TODO
	attributesSet_[transferRate_] = true;
//cout << __FILE__ << ":" << __LINE__ << " transferRateSet"<<endl;
	if (customerIsReady()) InjectActivityReactorNew();
}

void
Customer::CustomerReactor::onShipmentSize(PackageCount shipmentSize)
{
	//TODO
	attributesSet_[shipmentSize_] = true;
//cout << __FILE__ << ":" << __LINE__ << " shipmentSizeSet"<<endl;	
	if (customerIsReady()) InjectActivityReactorNew();
}

void
Customer::CustomerReactor::onDestination(Customer::Ptr destination)
{
	//TODO
	attributesSet_[dest_] = true;
//cout << __FILE__ << ":" << __LINE__ << " destinationSet"<<endl;	
	if (customerIsReady()) InjectActivityReactorNew();
}


void
Customer::CustomerReactor::InjectActivityReactorNew()
{
	if (createdInjectActivityReactor_) return;
//cout << __FILE__ << ":" << __LINE__ << " "<< notifier()->name() << " making inject reactor" << endl;
	Activity::Ptr activity = activityManager_->activityNew("InjectActivity");
	activity->lastNotifieeIs(
		new InjectActivityReactor(activityManager_, activity.ptr(),
			notifier().ptr(), 24.0 / (double) notifier()->transferRate().value(), network_)
		);
	activity->nextTimeIs(activityManager_->now().value());
	activity->statusIs(Activity::nextTimeScheduled);
	createdInjectActivityReactor_ = true;

	//delete this; // we no longer have any use for this
}


void
Terminal::segmentIs(const Segment::PtrConst &s)
{
	if (s->mode() == vehicleType()) {
		segments_.push_back(s);
	}
	else {
		cerr << "Terminal::segmentIs() trying to add segment "<< s->name()
		<<" of mode "<< Segment::modeName(s->mode()) << " to terminal of mode "
		<< Segment::modeName(vehicleType()) << endl;
	}
}

size_t
Path::locationIndex(const Location::PtrConst &location) const
{
	for (vector<Part>::const_iterator itr = path_.begin(); itr != path_.end(); ++itr) {
		if (itr->type == segment()) continue;
		if (itr->loc->name() == location->name())
			return itr - path_.begin();
	}
	throw Fwk::RangeException("value=range()");
}

Path::Membership
Path::locationMembershipStatus(const Location::PtrConst &location) const
{
	if(usedLocations_.find(location->name()) != usedLocations_.end())  {
		return Path::isMember();
	}
	return Path::notMember();
}

void
Path::endLocationIs(const Location::PtrConst &newEnd)
{
	if(numLocations() - numSegments() == 0 && newEnd) {
		if(!start()) start_ = newEnd;
		endLocation_ = newEnd;
		if(endSegmentPart_.type != Path::nil()) {
			path_.push_back(endSegmentPart_);

			if (expedited() && endSegmentPart_.seg->expediteSupport() != Segment::expediteSupported()) {
				// first unexpedited segment makes this path unexpedited
				expeditedIs(Segment::expediteNotSupported());
			}
			else {
				Dollars segCost = calculateCost(endSegment_);
				Hours segHours = calculateHours(endSegment_);
				Mile segDistance = endSegment_->length();
				costIs( Dollars(cost().value() + segCost.value()) );
				hoursIs( Hours(hours().value() + segHours.value()) );
				distanceIs( Mile(distance().value() + segDistance.value()) );
			}
		}
		path_.push_back(Part(endLocation_));
		usedLocations_.insert(endLocation_->name());
		numLocationsIs(numLocations() + 1);
	}
}

void
Path::endSegmentIs(const Segment::PtrConst &seg)
{
	if(numLocations() - numSegments() > 0 && seg) {
		endSegment_ = seg;
		endSegmentPart_ = Part(endSegment_);
		numSegmentsIs(numSegments() + 1);
	}
}

void
Path::expeditedIs(Segment::ExpediteSupport es)
{
	if (es == Segment::expediteNotSupported() && expedited_ == Segment::expediteSupported()) {
		expedited_ = es;
		// recalc time and cost
		float totalCost = 0.f;
		float totalHours = 0.f;
		float totalDistance = 0.f;
		for(size_t i = 0; i < path_.size(); i++) {
			Part part = path_[i];
			if(part.type == Path::segment()) {
				Segment::PtrConst segment = part.seg;
				totalCost += calculateCost(segment).value();
				totalHours += calculateHours(segment).value();
				totalDistance += segment->length().value();
			}
		}
		costIs( Dollars(totalCost) );
		hoursIs( Hours(totalHours) );
		distanceIs( Mile(totalDistance) );
	}
	expedited_ = es;
}


Dollars
Path::calculateCost(const Segment::PtrConst &segment) const
{
	Segment::Mode mode = segment->mode();
	float costPerMile = fleet_->costPerMile(mode).value();
	if (expedited() == Segment::expediteSupported()) {
		costPerMile *= 1.5f; // raise cost by 50%
	}
	float miles = segment->length().value();
	float difficulty = segment->difficulty().value();
	return Dollars(costPerMile * miles * difficulty);
}

Hours
Path::calculateHours(const Segment::PtrConst &segment) const
{
	Segment::Mode mode = segment->mode();
	float milesPerHour = fleet_->speed(mode).value();
	if (expedited() == Segment::expediteSupported()) {
		milesPerHour *= 1.3f; // boost speed by 30%
	}
	float miles = segment->length().value();
	return Hours(miles / milesPerHour);
}

string
Path::stringValue() const
{
	stringstream output;
	for(size_t i = 0; i < path_.size(); i++) {
		Part part = path_[i];
		if (part.type == Path::location() && part.loc) {
			output << part.loc->name();
		}
		else if (part.type == Path::segment() && part.seg) {
			string segmentName = part.seg->name();
			string returnName = part.seg->returnSegment()->name();
			string length = part.seg->length().stringValue();
			output << "("<< segmentName <<":"<< length <<":"<< returnName <<")"<<" ";
		}
	}
	output << "\n";
	return output.str();
}

Shipment::Shipment(Customer::Ptr s, Customer::Ptr d, PackageCount p, Network *network): 
	Fwk::NamedInterface(Shipment::shipmentName(s, d)),
	network_(network),
	src_(s),
	dest_(d),
	load_(p)
	{
		path_ = network_->connectivity()->shipmentPath(name());
		if (!path_) {
			cerr <<__FILE__<<":"<<__LINE__<< ": ShipmentShipment() path not possible" << endl;
			throw Fwk::EntityNotFoundException("shipment path does not exist");
		}
	}

void
Connectivity::constraintsActiveDel()
{
	constraintsActiveIs(Connectivity::none());
}

void
Connectivity::constraintsActiveIs(int mask)
{
	if (mask == Connectivity::none()) {
		mask_ = 0;
		//cout << "CLEAR CONSTRAINTS";
		//printf("%#x\n", mask_);
	}
	else {
		if (mask & Connectivity::distance()) {
			mask_ = mask_ | Connectivity::distance();
			//cout << "CONSTRAIN DISTANCE";
			//printf("%#x\n", mask_);
		}
		if (mask & Connectivity::cost()) {
			mask_ = mask_ | Connectivity::cost();
			//cout << "CONSTRAIN COST";
			//printf("%#x\n", mask_);
		}
		if (mask & Connectivity::hours()) {
			mask_ = mask_ | Connectivity::hours();
			//cout << "CONSTRAIN TIME";
			//printf("%#x\n", mask_);
		}
		if (mask & Connectivity::expedited()) {
			mask_ = mask_ | Connectivity::expedited();
			//cout << "CONSTRAIN EXPEDITED";
			//printf("%#x\n", mask_);
		}
	}
}

void
queueOrStore(Path::Ptr path, const Location::PtrConst &end, queue<Path::Ptr> &pathQueue, vector<Path::Ptr> &expeditedPaths, vector<Path::Ptr> &notExpeditedPaths)
{
	if (path->end()->name() == end->name()) {
		if (path->expedited() == Segment::expediteSupported()) {
			expeditedPaths.push_back(path);
		}
		else if (path->expedited() == Segment::expediteNotSupported()) {
			notExpeditedPaths.push_back(path);
		}
	}
}

Path::Ptr
copyPath(Path::Ptr original, Fleet::PtrConst fleet, const Location::PtrConst &nextLocation, const Segment::PtrConst &nextSegment)
{
	Path::Ptr copy = Path::PathNew(fleet);
	*(copy.ptr()) = *(original.ptr());
	copy->endSegmentIs(nextSegment);
	copy->endLocationIs(nextLocation);
	return copy;
}

bool
Connectivity::isValidExplorePath(Path::Ptr path) const
{
	int constraints = constraintsActive();
	bool validPath = true;
	if (constraints & Connectivity::distance()) {
		if (path->distance() > constraintDistance()) {
			//cout << "NOT VALID PATH => DISTANCE" << endl;
			validPath = false;
		}
	}
	if (constraints & Connectivity::cost()) {
		if (path->cost() > constraintCost()) {
			//cout << "NOT VALID PATH => COST " << path->cost().value() << endl;
			validPath = false;
		}
	}
	if (constraints & Connectivity::hours()) {
		if (path->hours() > constraintHours()) {
			//cout << "NOT VALID PATH => TIME" << endl;
			validPath = false;
		}
	}
	if (constraints & Connectivity::expedited()) {
		if (constraintExpedited() == Segment::expediteSupported()
			&& path->expedited() != Segment::expediteSupported()) {
			//cout << "NOT VALID PATH => EXPEDITED" << endl;
			validPath = false;
		}
	}
	return validPath;
}

bool
Connectivity::isValidExplorePathNotExpedited(Path::Ptr one, Path::Ptr two) const
{
	one->expeditedIs(Segment::expediteNotSupported());
	if (isValidExplorePath(one)) {
		*(two.ptr()) = *(one.ptr());
		return true;
	}
	return false;
}


vector<string>
Connectivity::paths(SearchPattern pattern) const
{
	typedef Location::SegmentIteratorConst SegmentIteratorConst;

	Path::Ptr startPath = Path::PathNew(fleet());
	startPath->endLocationIs(constraintStart());

	queue<Path::Ptr> pathQueue;
	pathQueue.push(startPath);

	vector<Path::Ptr> notExpeditedPaths;
	vector<Path::Ptr> expeditedPaths;
	vector<Path::Ptr> completedPaths;

	while(pathQueue.size() > 0) {
		Path::Ptr curr = pathQueue.front();
		Location::PtrConst jumpingOffLocation = curr->end();
		pathQueue.pop();

		SegmentIteratorConst 	
			beginSeg = jumpingOffLocation->segmentsIteratorConstBegin(),
			endSeg = jumpingOffLocation->segmentsIteratorConstEnd();

		for(SegmentIteratorConst it = beginSeg; it != endSeg; ++it) {
			Segment::PtrConst nextSegment = (*it);
			if (!nextSegment->returnSegment()) continue;
			Location::PtrConst nextLocation = nextSegment->returnSegment()->source();

			if (pattern == Connectivity::explore()) {
				if (curr->locationMembershipStatus(nextLocation) == Path::notMember()) {
					Path::Ptr copy = copyPath(curr, fleet(), nextLocation, nextSegment);
					Path::Ptr copy2 = copyPath(curr, fleet(), nextLocation, nextSegment);
					if (isValidExplorePath(copy) || isValidExplorePathNotExpedited(copy, copy2)) {
						if (copy->expedited() == Segment::expediteSupported()) {
							expeditedPaths.push_back(copy);
						}
						else if (copy->expedited() == Segment::expediteNotSupported()) {
							notExpeditedPaths.push_back(copy);
						}
						pathQueue.push(copy2);
					}
					else { 
						//delete copy;
						//delete copy2;
					}
				}
			}
			else if (pattern == Connectivity::connect()) {
				if (curr->locationMembershipStatus(nextLocation) == Path::notMember()) {
					Path::Ptr copy = copyPath(curr, fleet(), nextLocation, nextSegment);
					if (copy->end()->name() == constraintEnd()->name()) {
						if (copy->expedited() == Segment::expediteSupported()) {
							expeditedPaths.push_back(copy);
						}
						else if (copy->expedited() == Segment::expediteNotSupported()) {
							notExpeditedPaths.push_back(copy);
						}
					}
					else pathQueue.push(copy);
				}
			}
		}
		//delete curr;
	}

	for(size_t i = 0; i < notExpeditedPaths.size(); i++) {
		completedPaths.push_back(notExpeditedPaths[i]);
	}

	for(size_t i = 0; i < expeditedPaths.size(); i++) {
		if (pattern == Connectivity::connect()) {
			Path::Ptr unexpeditedCopy = Path::PathNew(fleet());
			*(unexpeditedCopy.ptr()) = *(expeditedPaths[i].ptr());
			unexpeditedCopy->expeditedIs(Segment::expediteNotSupported());
			completedPaths.push_back(unexpeditedCopy);
		}
		completedPaths.push_back(expeditedPaths[i]);
	}

	return stringifyPaths(pattern, completedPaths);
}

vector<string>
Connectivity::stringifyPaths(SearchPattern pattern, vector<Path::Ptr> &completedPaths) const
{
	vector<string> completed;
	for(size_t i = 0; i < completedPaths.size(); i++) {
		Path::Ptr path = completedPaths[i];
		stringstream pathString;

		if (pattern == Connectivity::connect()) {
			pathString << path->cost().stringValue() << " " << path->hours().stringValue() << " ";
			if (path->expedited() == Segment::expediteSupported()) {
				pathString <<"yes";
			}
			else if (path->expedited() == Segment::expediteNotSupported()) {
				pathString <<"no";	
			}
			pathString << ";" << " ";
		}
		pathString << path->stringValue();
		completed.push_back(pathString.str());
	}

	//for(size_t i = 0; i < completedPaths.size(); i++) {
	//	delete completedPaths[i];
	//}
	completedPaths.erase(completedPaths.begin(), completedPaths.end());
	return completed;
}

class PathDistanceComp {
public:
	PathDistanceComp() {}

	bool operator()(Path::Ptr a, Path::Ptr b) const {
		return (a->distance() > b->distance());
	}
};

Path::Ptr Connectivity::BFSShortestPath(Location::PtrConst &startLoc, Location::PtrConst &endLoc){
	typedef Location::SegmentIteratorConst SegmentIteratorConst;

	Path::Ptr startPath = Path::PathNew(fleet());
	startPath->endLocationIs(startLoc);

	
	queue<Path::Ptr> pathQueue;
	pathQueue.push(startPath);


	while(pathQueue.size() > 0) {
		Path::Ptr curr = pathQueue.front();
		Location::PtrConst jumpingOffLocation = curr->end();
		pathQueue.pop();

		SegmentIteratorConst 	
			beginSeg = jumpingOffLocation->segmentsIteratorConstBegin(),
			endSeg = jumpingOffLocation->segmentsIteratorConstEnd();

		for(SegmentIteratorConst it = beginSeg; it != endSeg; ++it) {
			Segment::PtrConst nextSegment = (*it);
			if (!nextSegment->returnSegment()) continue;
			Location::PtrConst nextLocation = nextSegment->returnSegment()->source();

			if (curr->locationMembershipStatus(nextLocation) == Path::notMember()) {
				Path::Ptr copy = copyPath(curr, fleet(), nextLocation, nextSegment);
				if (copy->end()->name() == endLoc->name()) {
					return copy;
				}
				//don't push onto the queue if it's a customer
				else if (nextLocation->locationType() != Location::customer()) pathQueue.push(copy);
			}
		}
	}
	return Path::Ptr();
}

Path::Ptr Connectivity::DijkstraShortestPath(Location::PtrConst &startLoc, Location::PtrConst &endLoc){
	typedef Location::SegmentIteratorConst SegmentIteratorConst;

	Path::Ptr startPath = Path::PathNew(fleet());
	startPath->endLocationIs(startLoc);

	priority_queue<Path::Ptr, vector<Path::Ptr>, PathDistanceComp> pathQueue;
	pathQueue.push(startPath);

	Path::Ptr minimumCostPath;

	while(pathQueue.size() > 0) {
		Path::Ptr curr = pathQueue.top();
		Location::PtrConst jumpingOffLocation = curr->end();
		pathQueue.pop();

		SegmentIteratorConst 	
			beginSeg = jumpingOffLocation->segmentsIteratorConstBegin(),
			endSeg = jumpingOffLocation->segmentsIteratorConstEnd();

		for(SegmentIteratorConst it = beginSeg; it != endSeg; ++it) {
			Segment::PtrConst nextSegment = (*it);
			if (!nextSegment->returnSegment()) continue;
			Location::PtrConst nextLocation = nextSegment->returnSegment()->source();

			if (curr->locationMembershipStatus(nextLocation) == Path::notMember()) {
				Path::Ptr copy = copyPath(curr, fleet(), nextLocation, nextSegment);
				if (copy->end()->name() == endLoc->name()) {
					if (!minimumCostPath) minimumCostPath = copy;
					else if (copy->cost() < minimumCostPath->cost()) minimumCostPath = copy;
				}
				//don't push onto the queue if it's not a customer
				else if (nextLocation->locationType() != Location::customer()) pathQueue.push(copy);
			}
		}
	}
	return minimumCostPath;
}

map<string, Path::Ptr> Connectivity::routes(RoutingMethod rm){
	map<string, Path::Ptr> routeMap;
	vector<Location::PtrConst> locs = network_->locations();

	for(size_t i = 0; i < locs.size(); i++){
		for(size_t j = 0; j < locs.size(); j++){
			if (i == j) continue;
			if (locs[i]->locationType() != Location::customer()) continue;
			if (locs[j]->locationType() != Location::customer()) continue;

			Path::Ptr shortPath;
			

			if (rm == dijkstra()) shortPath = DijkstraShortestPath(locs[i], locs[j]);
//			if (shortPath) cout<<__FILE__<<": Dijkstra "<< shortPath->stringValue();
			else if (rm == bfs()){ shortPath = BFSShortestPath(locs[i], locs[j]);
//				if(shortPath) cout<<__FILE__<<": BFS: "<< shortPath->stringValue();
			}

			//cout<<__FILE__<<": BFS: "<< BFSShortestPath(locs[i], locs[j])->stringValue();

			if (shortPath){
				stringstream key;
				key << locs[i]->name() << ":" << locs[j]->name(); 
				routeMap[key.str()] = shortPath;
			}
		}
	}
	return routeMap;
}

Location::Ptr
Network::location(const Fwk::String &name) {
	map<Fwk::String, Location::Ptr>::iterator found = locations_.find(name);
	if(found == locations_.end()) {
		return Location::Ptr();
	}
	
	return found->second;
}

void
Network::expediteSupportIs(Fwk::String name, Segment::ExpediteSupport supported)
{
	Segment::Ptr seg = segment(name);
	if(seg) {
		int step = 0;
		if(supported == Segment::expediteSupported()) step = 1;
		else if(supported == Segment::expediteNotSupported()) step = -1;

		// tell all the notifiees
		if(notifiees()) {
			for(NotifieeIterator n=notifieeIter(); n.ptr(); ++n) {
				try { n->onNumExpediteSupportedSegments(step); }
				catch(...) { cerr << "Network::expediteSupportIs() notification for " << name << " unsuccessful" << endl; }
			}
		}
		seg->expediteSupportIs(supported);
	}
}

Segment::Ptr
Network::segmentNew(Fwk::String name, Segment::Mode mode)
{
	// check to see if it already exists
	Segment::Ptr seg = segment(name);
	if(seg) {
		cerr << "Network::segmentNew() Segment name " << name << " already in use!" << endl;
		return Segment::Ptr();
	}
	// if not then create it and add to network
	else {
		seg = Segment::SegmentNew(name, mode, this);
		segmentIs(name, seg);
	}
	// tell all the notifiees
	if(notifiees()) {
		for(NotifieeIterator n=notifieeIter(); n.ptr(); ++n) {
			try { n->onSegmentNew(seg); }
			catch(...) { cerr << "Network::segmentNew() notification for " << name << " unsuccessful" << endl; }
		}
	}
	return seg;
}

Segment::Ptr
Network::segmentDel(Fwk::String name)
{
	// try to retrieve
	Segment::Ptr seg = segment(name);
	size_t n = segments_.erase(name);
	if(n == 0) return 0;

	seg->sourceIs(Location::Ptr());

	// tell all the notifiees
	if(notifiees()) {
		for(NotifieeIterator n=notifieeIter(); n.ptr(); ++n) {
			try { n->onSegmentDel(seg); }
			catch(...) { cerr << "Network::segmentDel() notification for " << name << " unsuccessful" << endl; }
		}
	}
	return seg;
}

void
Network::locationSegmentsDel(Location::Ptr location)
{
	typedef Location::SegmentIterator SegmentIterator;
	SegmentIterator begin = location->segmentsIteratorBegin();
	SegmentIterator end = location->segmentsIteratorEnd();

	vector<Segment::Ptr> segments;
	for(SegmentIterator it = begin; it < end; ++it) {
		Segment::PtrConst constPtr = (*it);
		Segment::Ptr ptr = const_cast<Segment*>(constPtr.ptr());
		segments.push_back(ptr);
	}

	for(size_t i = 0; i < segments.size(); i++) {
		//segmentDel(segments[i]->name());
		segments[i]->sourceIs(Location::Ptr());
	}
}

Customer::Ptr
Network::customerNew(Fwk::String name)
{
	// check to see if it already exists
	Location::Ptr cust = location(name);
	Customer::Ptr customer;

	if(cust) {
		cerr << "Network::customerNew() Customer name " << name << " already in use!" << endl;
		return Customer::Ptr();
	}
	// if not then create it and add to network
	else {
		customer = Customer::CustomerNew(name, this);
		locationIs(name, customer);
	}
	// tell all the notifiees
	if(notifiees()) {
		for(NotifieeIterator n=notifieeIter(); n.ptr(); ++n) {
			//Customer::Ptr cust = dynamic_cast<Customer *>(customer.ptr());
			try { n->onCustomerNew(customer); }
			catch(...) { cerr << "Network::customerNew() notification for " << name << " unsuccessful" << endl; }
		}
	}
	return customer;
}

Customer::Ptr
Network::customerDel(Fwk::String name)
{
	// try to retrieve
	Location::Ptr customer = location(name);
	size_t n = locations_.erase(name);
	if(n == 0) return 0;

	locationSegmentsDel(customer);
	
	// tell all the notifiees
	Customer::Ptr cust = dynamic_cast<Customer *>(customer.ptr());
	if(notifiees()) {
		for(NotifieeIterator n=notifieeIter(); n.ptr(); ++n) {
			try { n->onCustomerDel(cust); }
			catch(...) { cerr << "Network::customerDel() notification for " << name << " unsuccessful" << endl; }
		}
	}
	return cust;
}

Location::Ptr
Network::portNew(Fwk::String name)
{
	// check to see if it already exists
	Location::Ptr port = location(name);
	if(port) {
		cerr << "Network::portNew() Port name " << name << " already in use!" << endl;
		return Port::Ptr();
	}
	// if not then create it and add to network
	else {
		port = Port::PortNew(name, this);
		locationIs(name, port);
	}
	// tell all the notifiees
	if(notifiees()) {
		for(NotifieeIterator n=notifieeIter(); n.ptr(); ++n) {
			Port::Ptr prt = dynamic_cast<Port *>(port.ptr());
			try { n->onPortNew(prt); }
			catch(...) { cerr << "Network::portNew() notification for " << name << " unsuccessful" << endl; }
		}
	}
	return port;
}

Location::Ptr
Network::portDel(Fwk::String name)
{
	// try to retrieve
	Location::Ptr port = location(name);
	size_t n = locations_.erase(name);
	if(n == 0) return 0;

	locationSegmentsDel(port);

	// tell all the notifiees
	if(notifiees()) {
		for(NotifieeIterator n=notifieeIter(); n.ptr(); ++n) {
			Port::Ptr prt = dynamic_cast<Port *>(port.ptr());
			try { n->onPortDel(prt); }
			catch(...) { cerr << "Network::portDel() notification for " << name << " unsuccessful" << endl; }
		}
	}
	return port;
}

Location::Ptr
Network::terminalNew(Fwk::String name, Segment::Mode vehicle_type)
{
	// check to see if it already exists
	Location::Ptr terminal = location(name);
	if(terminal) {
		cerr << "Network::terminalNew() Terminal name " << name << " already in use!" << endl;
		return Terminal::Ptr();
	}
	// if not then create it and add to network
	else {
		terminal = Terminal::TerminalNew(name, vehicle_type, this);
		locationIs(name, terminal);
	}
	// tell all the notifiees
	if(notifiees()) {
		for(NotifieeIterator n=notifieeIter(); n.ptr(); ++n) {
			Terminal::Ptr term = dynamic_cast<Terminal *>(terminal.ptr());
			try { n->onTerminalNew(term); }
			catch(...) { cerr << "Network::terminalNew() notification for " << name << " unsuccessful" << endl; }
		}
	}
	return terminal;
}

Location::Ptr
Network::terminalDel(Fwk::String name)
{
	// try to retrieve
	Location::Ptr terminal = location(name);
	size_t n = locations_.erase(name);
	if(n == 0) return 0;

	locationSegmentsDel(terminal);

	// tell all the notifiees
	if(notifiees()) {
		for(NotifieeIterator n=notifieeIter(); n.ptr(); ++n) {
			Terminal::Ptr term = dynamic_cast<Terminal *>(terminal.ptr());
			try { n->onTerminalDel(term); }
			catch(...) { cerr << "Network::terminalDel() notification for " << name << " unsuccessful" << endl; }
		}
	}
	return terminal;
}

Fleet::Ptr
Network::fleetNew(Fwk::String name)
{
	// check to see if it already exists
	if(fleet_) {
		cerr << "Network::fleetNew() Fleet name '" << fleet_->name() << "' already in use!" << endl;
		return fleet_;
	}
	// if not then create it and add to network
	else {
		fleet_ = Fleet::FleetNew(name, this);
	}
	return fleet_;
}

Fleet::Ptr
Network::fleetDel(Fwk::String name)
{
	Fleet::Ptr fleet = fleet_;
	if(fleet) {
		fleet_ = Fleet::Ptr();
	}
	return fleet;
}

Fwk::Ptr<Statistics>
Network::statisticsNew(Fwk::String name)
{
	// check to see if it already exists
	if(statistics_) {
		cerr << "Network::statisticsNew() Statistics name '" << statistics_->name() << "' already in use!" << endl;
		return statistics_;
	}
	// if not then create it and add to network
	else {
		statistics_ = Statistics::StatisticsNew(name, this);
	}
	return statistics_;
}

Fwk::Ptr<Statistics>
Network::statisticsDel(Fwk::String name)
{
	Fwk::Ptr<Statistics> stats = statistics_;
	if(stats) {
		statistics_ = Fwk::Ptr<Statistics>();
	}
	return stats;
}

Connectivity::Ptr
Network::connectivityNew(Fwk::String name)
{
	// check to see if it already exists
	if(connectivity_) {
		cerr << "Network::connectivityNew() Connectivity name '" << connectivity_->name() << "' already in use!" << endl;
		return connectivity_;
	}
	// if not then create it and add to network
	else {
		connectivity_ = Connectivity::ConnectivityNew(name, this);
	}
	//cout <<__FILE__<<":"<< __LINE__<< " network=" << this << endl;
	//cout <<__FILE__<<":"<< __LINE__<< " conn=" << connectivity_.ptr() << endl;
	return connectivity_;
}

Connectivity::Ptr
Network::connectivityDel(Fwk::String name)
{
	Connectivity::Ptr conn = connectivity_;
	if(conn) {
		connectivity_ = Connectivity::Ptr();
	}
	return conn;
}
Shipment::Ptr
Network::shipmentNew(Customer::Ptr s, Customer::Ptr d, PackageCount p)
{
	Shipment::Ptr shipment = Shipment::ShipmentNew(s, d, p, this);
	// tell all the notifiees
	if(notifiees()) {
		for(NotifieeIterator n=notifieeIter(); n.ptr(); ++n) {
			try { n->onShipmentNew(shipment); }
			catch(...) { cerr << "Network::shipmentNew() notification for " << shipment->name() << " unsuccessful" << endl; }
		}
	}
	return shipment;	
}

void
Network::NotifieeConst::notifierIs(const Network::PtrConst& _notifier) {
   Network::Ptr notifierSave(const_cast<Network *>(notifier_.ptr()));
   if(_notifier==notifier_) return;
   notifier_ = _notifier;
   if(notifierSave) {
	  notifierSave->deleteNotifiee(this);
   }
   if(_notifier) {
	  _notifier->newNotifiee(this);
   }
   if(isNonReferencing_) {
	  if(notifierSave) notifierSave->newRef();
	  if(notifier_) notifier_->deleteRef();
   }
}

Network::NotifieeConst::~NotifieeConst() {
   if(notifier_) {
	  notifier_->deleteNotifiee(this);
   }
   if(notifier_&&isNonReferencing()) notifier_->newRef();
}

string
Statistics::simulationShipmentStats() const
{
	stringstream output;
	output << " --- Shipments --- " << endl;
    output << "# Shipments enroute   : " << numShipments(Statistics::enroute()) << endl;
    output << "# Shipments delivered : " << numShipments(Statistics::delivered()) << endl;
    output << "# Shipments dropped   : " << numShipments(Statistics::dropped()) << endl;
    return output.str();
}

string
Statistics::simulationStatisticsOutput() const
{
//cout << __FILE__ << ":" << __LINE__ << endl;
	stringstream output;
	output << "===== Simulation Results =====" << endl;
	output << " --- Locations --- " << endl;
    output << "# Customers.......: " << numCustomers() << endl;
    output << "# Ports...........: " << numPorts() << endl;
    output << "# Truck terminals.: " << numTerminals(Segment::truck()) << endl;
    output << "# Boat terminals..: " << numTerminals(Segment::boat()) << endl;
    output << "# Plane terminals.: " << numTerminals(Segment::plane()) << endl;
    output << endl;

    output << " --- Segments --- " << endl;
    output << "# Truck segments : " << numSegments(Segment::truck()) << endl;
    output << "# Boat segments  : " << numSegments(Segment::boat()) << endl;
    output << "# Plane segments : " << numSegments(Segment::plane()) << endl;
    output << endl;

    output << " --- Shipments --- " << endl;
    output << "# Shipments enroute   : " << numShipments(Statistics::enroute()) << endl;
    output << "# Shipments delivered : " << numShipments(Statistics::delivered()) << endl;
    output << "# Shipments dropped   : " << numShipments(Statistics::dropped()) << endl;
    output << endl;

    output << " --- Shipment Averages --- " << endl;
    Network::Ptr network = network_;
    vector<Segment::PtrConst> segments = network->segments();
	size_t numReceived = 0, numWait = 0, numRefused = 0;
    for (size_t i = 0; i < segments.size(); i++) {
    	Segment::PtrConst segment = dynamic_cast<Segment const*>(segments[i].ptr());
    	numReceived += segment->numShipmentsReceived().value();
    	numWait += segment->numShipmentsToldToWait().value();
    	numRefused += segment->numShipmentsRefused().value();
    }

	output
	<< "AvgReceived=" << (float) numReceived / (float) segments.size() << " "
	<< "AvgToldToWait=" << (float) numWait / (float) segments.size() << " "
	<< "AvgRefused=" << (float) numRefused / (float) segments.size() << " "
	<< endl << endl << endl;

    output << " --- Customers --- " << endl;
    vector<Location::PtrConst> locations = network->locations();
    for (size_t i = 0; i < locations.size(); i++) {
    	if (locations[i]->locationType() != Location::customer()) {
    		continue;
    	}
    	Customer::PtrConst customer = dynamic_cast<Customer const*>(locations[i].ptr());
    	//output << "# Shipments enroute   : " << numShipments(Statistics::enroute()) << endl;
    	output << "'" << customer->name() << "'->'";
    	if (customer->destination()) output << customer->destination()->name() << "' : ";
    	else output << "' : ";
    	output << "Received=" << customer->shipmentsReceived().value() << " "
    	<< "avgLatency=" << customer->avgLatency().value() << " "
    	<< "totalCost=" << customer->totalCost().value() << " "
    	<< endl;
    }
    output << endl << endl;

    output << " --- Segments --- " << endl;

    for (size_t i = 0; i < segments.size(); i++) {
    	Segment::PtrConst segment = dynamic_cast<Segment const*>(segments[i].ptr());

    	output << segment->name() << " : "
    	<< "Received=" << segment->numShipmentsReceived().value() << " "
    	<< "ToldToWait=" << segment->numShipmentsToldToWait().value() << " "
    	<< "Refused=" << segment->numShipmentsRefused().value() << " "
    	<< endl;
    }
    return output.str();
    //output << "Expediting %     : " << stats->attribute("expedite percentage") << endl;
}

void
Statistics::onSegmentNew(Segment::Ptr segment)
{
	Segment::Mode type = segment->mode();
	numSegmentsIs(type, numSegments(type) + 1);
}

void
Statistics::onCustomerNew(Customer::Ptr customer)
{
	numCustomersIs(numCustomers() + 1);
}

void
Statistics::onPortNew(Port::Ptr port)
{
	numPortsIs(numPorts() + 1);
}
void
Statistics::onTerminalNew(Terminal::Ptr terminal)
{
	Segment::Mode type = terminal->vehicleType();
	numTerminalsIs(type, numTerminals(type) + 1);
}

void
Statistics::onSegmentDel(Segment::Ptr segment)
{
	Segment::Mode type = segment->mode();
	numSegmentsIs(type, numSegments(type) - 1);
	if (segment->expediteSupport() == Segment::expediteSupported()) {
		numExpeditedSegments_ -= 1;
	}
}

void
Statistics::onCustomerDel(Customer::Ptr customer)
{
	numCustomersIs(numCustomers() - 1);
}

void
Statistics::onPortDel(Port::Ptr port)
{
	numPortsIs(numPorts() - 1);
}

void
Statistics::onTerminalDel(Terminal::Ptr terminal)
{
	Segment::Mode type = terminal->vehicleType();
	numTerminalsIs(type, numTerminals(type) - 1);
}

void
Statistics::deliveredShipmentIs(Shipment::Ptr shipment)
{
	map<string, ShippingRecord>::iterator found = shipmentRecords_.find(shipment->name());
	found->second.numEnRouteInc(-1);
	found->second.numDeliveredInc();
	numShipmentsIs(Statistics::enroute(), numShipments(Statistics::enroute()) - 1);	
	numShipmentsIs(Statistics::delivered(), numShipments(Statistics::delivered()) + 1);
}

void
Statistics::droppedShipmentIs(Shipment::Ptr shipment)
{
	map<string, ShippingRecord>::iterator found = shipmentRecords_.find(shipment->name());
	found->second.numEnRouteInc(-1);
	found->second.numDroppedInc();
	numShipmentsIs(Statistics::enroute(), numShipments(Statistics::enroute()) - 1);	
	numShipmentsIs(Statistics::dropped(), numShipments(Statistics::dropped()) + 1);
}

void
Statistics::onShipmentNew(Shipment::Ptr shipment)
{
	numShipments_[Statistics::enroute()]++;
	map<string, ShippingRecord>::iterator found = shipmentRecords_.find(shipment->name());
	if (found == shipmentRecords_.end()) {
		ShippingRecord record;
		record.numEnRouteInc();
		shipmentRecords_[shipment->name()] = record;
	}
	else {
		found->second.numEnRouteInc();
	}
}

void
Statistics::onNumExpediteSupportedSegments(int n)
{
	//cout << "step=" << n << endl;
	numExpeditedSegments_ += n;
}

float
Statistics::percentExpeditedSegments()
{
	float total = (float) numSegments_[Segment::truck()]
		+ (float) numSegments_[Segment::boat()]
		+ (float) numSegments_[Segment::plane()];
		//cout << "num="<< numExpeditedSegments_ << " total=" << total << endl;
	return (float) numExpeditedSegments_ / total * 100.f;
}

} /* end namespace */
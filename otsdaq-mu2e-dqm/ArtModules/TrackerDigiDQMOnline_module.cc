// Online tracker digi DQM: fills mu2e::TrackerDigiDQM (Offline/TrackerDQM)
// from StrawDigis and ships what its `digi` table publishes through
// HistoSender (detail/DQMPublisher.hh). The histograms and their binning are
// the client's, shared with the offline DqmTrackerDigi module (Mu2e/DQM);
// this module owns only I/O. A display of its own would go here.

#include <string>

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Principal/SubRun.h"
#include "art_root_io/TFileService.h"
#include "fhiclcpp/types/Table.h"

#include "otsdaq-mu2e-dqm/ArtModules/detail/DQMPublisher.hh"

#include "Offline/DQMHelpers/inc/DQMHistSetConfig.hh"
#include "Offline/RecoDataProducts/inc/StrawDigi.hh"
#include "Offline/TrackerDQM/inc/TrackerDigiDQM.hh"

namespace
{
// A client's `hists` table, validated; an absent table is the job-only default.
mu2e::DQMHistSet::Config clientHists(fhicl::ParameterSet const& ps, const char* key)
{
	return mu2e::toConfig(
	    fhicl::Table<mu2e::DQMClientFhicl>(ps.get<fhicl::ParameterSet>(key, {}))().hists());
}
}  // namespace

namespace ots
{

class TrackerDigiDQMOnline : public art::EDAnalyzer
{
  public:
	explicit TrackerDigiDQMOnline(fhicl::ParameterSet const& ps);

  private:
	void analyze(art::Event const& event) override;
	void beginJob() override;
	void beginRun(art::Run const& run) override;
	void beginSubRun(art::SubRun const& subRun) override;
	void endSubRun(art::SubRun const& subRun) override;
	void endJob() override;

	art::InputTag digiTag_;
	std::string   outputTag_;
	int           diagLevel_;

	art::ServiceHandle<art::TFileService> tfs_;
	mu2e::TrackerDigiDQM                  dqm_;
	DQMPublisher                          publisher_;  // address, port, sendHists, sendIntervalSec
};

TrackerDigiDQMOnline::TrackerDigiDQMOnline(fhicl::ParameterSet const& ps)
    : art::EDAnalyzer(ps)
    , digiTag_(ps.get<std::string>("digiTag", "TrigMakeSD"))
    , outputTag_(ps.get<std::string>("outputTag", "TrackerDigiDQM"))
    , diagLevel_(ps.get<int>("diagLevel", 0))
    , dqm_(clientHists(ps, "digi"))
    , publisher_(ps, "trk")
{
}

void TrackerDigiDQMOnline::beginJob()
{
	dqm_.Book(tfs_->mkdir(outputTag_));
	publisher_.add(dqm_);
	publisher_.connect();
}

// The DQM process can outlive a run: a new run starts from empty histograms.
void TrackerDigiDQMOnline::beginRun(art::Run const&) { dqm_.ResetForNewRun(); }

void TrackerDigiDQMOnline::beginSubRun(art::SubRun const& subRun)
{
	dqm_.BeginSubRun(static_cast<int>(subRun.run()), static_cast<int>(subRun.subRun()));
}

void TrackerDigiDQMOnline::endSubRun(art::SubRun const&) { dqm_.EndSubRun(); }

void TrackerDigiDQMOnline::analyze(art::Event const& event)
{
	// A missing collection is counted, not fatal: the monitor must keep running.
	art::Handle<mu2e::StrawDigiCollection> digis;
	event.getByLabel(digiTag_, digis);
	const mu2e::StrawDigiCollection empty;
	if(!digis.isValid())
		dqm_.diag().Count("missingStrawDigis", "no StrawDigiCollection " + digiTag_.encode());
	dqm_.Fill(digis.isValid() ? *digis : empty);

	if(publisher_.due())
		publisher_.send();
}

void TrackerDigiDQMOnline::endJob()
{
	dqm_.EndJob();
	publisher_.send();
	if(diagLevel_ > 0)
		publisher_.logPublished();
	publisher_.disconnect();
}

}  // namespace ots

DEFINE_ART_MODULE(ots::TrackerDigiDQMOnline)

// -*- C++ -*-
//
// Package:    HepMCSplitter
// Class:      HepMCSplitter
// 
/**\class HepMCSplitter HepMCSplitter.cc MyAna/HepMCSplitter/src/HepMCSplitter.cc

 Description: <one line class summary>

 Implementation:
     <Notes on implementation>
*/
//
// Original Author:  Tomasz Maciej Frueboes
//         Created:  Wed Dec  9 16:14:56 CET 2009
// $Id: HepMCSplitter.cc,v 1.5 2011/06/24 13:52:42 aburgmei Exp $
//
//


// system include files
#include <memory>

// user include files
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/EDProducer.h"

#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"

#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include <DataFormats/RecoCandidate/interface/RecoCandidate.h>
#include <DataFormats/Candidate/interface/CompositeRefCandidate.h>
#include <DataFormats/MuonReco/interface/Muon.h>

#include <DataFormats/ParticleFlowCandidate/interface/PFCandidate.h>
#include <DataFormats/ParticleFlowCandidate/interface/PFCandidateFwd.h>
#include "DataFormats/TrackReco/interface/Track.h"
#include "DataFormats/Math/interface/deltaR.h"

#include "SimDataFormats/GeneratorProducts/interface/HepMCProduct.h"
#include <HepMC/GenEvent.h>
#include <HepMC/GenVertex.h>
#include <HepMC/GenParticle.h>

// class decleration
//

class HepMCSplitter : public edm::EDProducer {
   public:
      explicit HepMCSplitter(const edm::ParameterSet&);
      ~HepMCSplitter();

   private:
      virtual void beginJob() ;
      virtual void produce(edm::Event&, const edm::EventSetup&);
      virtual void endJob() ;
      
      
      
      int cntStableParticles(const HepMC::GenEvent * evtUE);
      // ----------member data ---------------------------
        edm::InputTag _input;
};

//
// constants, enums and typedefs
//


//
// static data member definitions
//

//
// constructors and destructor
//
HepMCSplitter::HepMCSplitter(const edm::ParameterSet& iConfig):
   _input(iConfig.getParameter<edm::InputTag>("input") )
{

  produces<edm::HepMCProduct>("UE");
  produces<edm::HepMCProduct>("Ztautau");
}

HepMCSplitter::~HepMCSplitter()
{
 
   // do anything here that needs to be done at desctruction time
   // (e.g. close files, deallocate resources etc.)

}


//
// member functions
//

// ------------ method called to produce the data  ------------
void
HepMCSplitter::produce(edm::Event& iEvent, const edm::EventSetup& iSetup)
{
  using namespace edm;
  using namespace reco;

  // Get the product 

  Handle<edm::HepMCProduct> prodIn;
  iEvent.getByLabel(_input, prodIn);

  

  //  find Z decay and prod vertex
  int ZprodVtx, ZdecayVtx = 0;
  bool vtxFound = false;
  HepMC::GenEvent::particle_const_iterator parIt, parItE;
  parIt = prodIn->GetEvent()->particles_begin();
  parItE = prodIn->GetEvent()->particles_end();
  for ( ;parIt != parItE; ++parIt){  
      int pdg = abs( (*parIt)->pdg_id() ) ;
      if (pdg == 23){
	ZdecayVtx = (*parIt)->end_vertex()->barcode();
	ZprodVtx = (*parIt)->production_vertex()->barcode();
        vtxFound=true;
        break;
      }
  }

  if ( !vtxFound) throw cms::Exception("Z vtx not found");
//   std::cout << " Found Z - prod vtx " << ZprodVtx 
//       << " decay vtx " << ZdecayVtx 
//       << std::endl;

  
  
  // iterate over all vertices, mark those comming from Z
  std::set<int> barcodesZ;
  barcodesZ.insert(ZdecayVtx);
  
  HepMC::GenEvent::vertex_const_iterator itVtx, itVtxE;
  itVtx = prodIn->GetEvent()->vertices_begin();
  itVtxE = prodIn->GetEvent()->vertices_end();

  for(;itVtx!=itVtxE;++itVtx){
    if ( barcodesZ.find( (*itVtx)->barcode() )!=barcodesZ.end()){
      
      // iterate over out particles. Copy their decay vertices.to Z vertices
      HepMC::GenVertex::particles_out_const_iterator parIt, parItE;
      parIt=(*itVtx)->particles_out_const_begin();
      parItE=(*itVtx)->particles_out_const_end();
      for ( ; parIt!=parItE; ++parIt){
        if ((*parIt)->end_vertex()){
          barcodesZ.insert((*parIt)->end_vertex()->barcode());
          //std::cout << " Inserted " << (*parIt)->end_vertex()->barcode() << std::endl;
        }
      }
      
    }
  }
  
  barcodesZ.insert(ZprodVtx);
  barcodesZ.insert(ZdecayVtx);
  
  //std::cout << " Z vertices identified "  << barcodesZ.size() << std::endl;

  // prepare final products
  HepMC::GenEvent * evtUE = new HepMC::GenEvent;
  HepMC::GenEvent * evtTauTau = new HepMC::GenEvent;
  
  // Copy the vertices
  itVtx = prodIn->GetEvent()->vertices_begin();
  itVtxE = prodIn->GetEvent()->vertices_end();
  int newBarcode = 1;
  
  std::map<int, HepMC::GenVertex*> barcodeMapUE;
  std::map<int, HepMC::GenVertex*> barcodeMapTauTau;
  for(;itVtx!=itVtxE;++itVtx){
    bool isZ = barcodesZ.find( (*itVtx)->barcode() )!=barcodesZ.end();
    
    bool leadToZ = false;
    if ( (*parIt)->production_vertex()  ) {
      leadToZ = std::abs( (*itVtx)->barcode()) < std::abs(ZprodVtx);
    }
    
    
    // special case - Z production vertex. Put in both
    if (isZ || (*itVtx)->barcode()==ZprodVtx || leadToZ ){
      HepMC::GenVertex* newvertex = new HepMC::GenVertex((*itVtx)->position(), 
                                               (*itVtx)->id(), 
                                               (*itVtx)->weights() );
      
      newvertex->suggest_barcode(  -(++newBarcode) );
      //std::cout << "Adding vtx " << (*itVtx)->barcode() << std::endl;
      barcodeMapTauTau[(*itVtx)->barcode()] = newvertex;
      evtTauTau->add_vertex(newvertex);
    } 
    
    if (!isZ || (*itVtx)->barcode()==ZprodVtx){
      HepMC::GenVertex* newvertex = new HepMC::GenVertex((*itVtx)->position(), 
                                             (*itVtx)->id(), 
                                               (*itVtx)->weights() );
      
      newvertex->suggest_barcode(  -(++newBarcode) );
      barcodeMapUE[ (*itVtx)->barcode()] = newvertex;
      evtUE->add_vertex(newvertex);
    }
  }
  
  
  
  //std::cout << " Vertices copied " << std::endl;
  

  
  // iterate over particles again. Copy and assign
  parIt = prodIn->GetEvent()->particles_begin();
  parItE = prodIn->GetEvent()->particles_end();
  for ( ;parIt != parItE; ++parIt){  
    
    // check if prod vertex is Z vertex.
    bool fromZprodVtx = false;
    if ( (*parIt)->production_vertex() ){
      fromZprodVtx = ( (*parIt)->production_vertex()->barcode() == ZprodVtx);
    }
    
    // check if from Z
    bool fromZ = false;
    if ( (*parIt)->production_vertex() ){
      fromZ = barcodesZ.find( (*parIt)->production_vertex()->barcode() )!=barcodesZ.end();
    }
    
    
    // incoming particle (proton) or interaction that lead to Z boson. Copy to both events
    bool leadToZ = false;
    //if ( (*parIt)->production_vertex()  && (*parIt)->end_vertex()) {
    if ( (*parIt)->end_vertex()) {
      //leadToZ = std::abs((*parIt)->production_vertex()->barcode()) < std::abs(ZprodVtx);
      leadToZ = std::abs((*parIt)->end_vertex()->barcode()) < std::abs(ZdecayVtx);
    }
    
    //if ( !(*parIt)->production_vertex() || leadToZ ) {
    if ( leadToZ ) {
    
      HepMC::GenParticle* newparticle = new HepMC::GenParticle(*(*parIt));
      newparticle->suggest_barcode(newBarcode++);
      if ( (*parIt)->end_vertex()){
        //XX Add xcheck if this barcode is in map
        barcodeMapUE[ (*parIt)->end_vertex()->barcode()]->add_particle_in(newparticle);
      } else {
        //std::cout << "XXX particle without vertices!" << std::endl;
      }
      
      newparticle = new HepMC::GenParticle(**parIt);
      newparticle->suggest_barcode(newBarcode++);
      if ( (*parIt)->end_vertex()){
        //XX Add xcheck if this barcode is in map
        //std::cout << "Trying: " << (*parIt)->end_vertex()->barcode() << std::endl;
        barcodeMapTauTau[ (*parIt)->end_vertex()->barcode()]->add_particle_in(newparticle);
      } else {
        //std::cout << "XXX particle without vertices!" << std::endl;
      }
    
    } else if (fromZprodVtx)     { // Special case - particle comes from Z prod vertex
      bool isZ = (*parIt)->pdg_id();
      HepMC::GenParticle* newparticle = new HepMC::GenParticle(**parIt);
      newparticle->suggest_barcode(newBarcode++);
      if (isZ){
        //XX Add xcheck if this barcode is in map
        if ( (*parIt)->end_vertex())  
          barcodeMapTauTau[(*parIt)->end_vertex()->barcode()]->add_particle_in(newparticle);
        if ((*parIt)->production_vertex()) 
          barcodeMapTauTau[(*parIt)->production_vertex()->barcode()]->add_particle_out(newparticle);
      } else {
        if ((*parIt)->end_vertex())  
          barcodeMapUE[(*parIt)->end_vertex()->barcode()]->add_particle_in(newparticle);
        if ((*parIt)->production_vertex()) 
          barcodeMapUE[(*parIt)->production_vertex()->barcode()]->add_particle_out(newparticle);
      }
    } else {
      // Generic case
      HepMC::GenParticle* newparticle = new HepMC::GenParticle(**parIt);
      newparticle->suggest_barcode(newBarcode++);
      if (fromZ){
          //XX Add xcheck if this barcode is in map
        if ((*parIt)->end_vertex())  
          barcodeMapTauTau[(*parIt)->end_vertex()->barcode()]->add_particle_in(newparticle);
        if ((*parIt)->production_vertex()) 
          barcodeMapTauTau[(*parIt)->production_vertex()->barcode()]->add_particle_out(newparticle);
      } else {
        if ((*parIt)->end_vertex())  
  //         std::cout << "trying " << (*parIt)->end_vertex()->barcode() << std::endl;
          barcodeMapUE[(*parIt)->end_vertex()->barcode()]->add_particle_in(newparticle);
        if ((*parIt)->production_vertex()) 
          barcodeMapUE[(*parIt)->production_vertex()->barcode()]->add_particle_out(newparticle);
      }
    }
  } // end particles iteration
  
//   std::cout << "XXXXXXXXXXXXXXXXXX" << std::endl;
  
  int orgCnt = cntStableParticles(prodIn->GetEvent());
  int tautauCnt = cntStableParticles(evtTauTau);
  int UEcnt = cntStableParticles(evtUE);
  if (orgCnt != tautauCnt + UEcnt){
    std::cout << " XXX Warning - wrong num particles - ORG: " 
      <<  orgCnt
      << " tautau " << tautauCnt
      << " UE "     << UEcnt
      << std::endl;
  }
  
//   std::cout << "=====================================\n";
//   std::cout << "EV Tau \n";
//   //evtTauTau->print();
//   std::cout << "=====================================\n";
//   std::cout << "EV org \n";
//   //prodIn->GetEvent()->print();
//   
//   std::cout << "=====================================\n";
//   std::cout << "EV EU \n";
  //   evtUE->print();
  

  std::auto_ptr<edm::HepMCProduct> prodUE(new edm::HepMCProduct());  
  prodUE->addHepMCData( evtUE );
  iEvent.put( prodUE, "UE");

  std::auto_ptr<edm::HepMCProduct> prodTauTau(new edm::HepMCProduct());  
  prodTauTau->addHepMCData( evtTauTau );
  iEvent.put( prodTauTau, "Ztautau");

  
}

// ------------ method called once each job just before starting event loop  ------------
void 
HepMCSplitter::beginJob()
{
}

// ------------ method called once each job just after ending the event loop  ------------
void 
HepMCSplitter::endJob() {
}

int HepMCSplitter::cntStableParticles(const HepMC::GenEvent * evt){
  
  int ret = 0;
  HepMC::GenEvent::particle_const_iterator parIt, parItE;
  parIt = evt->particles_begin();
  parItE = evt->particles_end();
  for ( ;parIt != parItE; ++parIt){  
    if ( (*parIt)->status()==1) ++ ret;
  
  }

  return ret;
}

//define this as a plug-in
DEFINE_FWK_MODULE(HepMCSplitter);
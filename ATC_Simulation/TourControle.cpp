#include "TourControle.hpp"
#include <sstream>

using namespace std;

TourControle::TourControle(int nombrePlacesParking)
    : pisteOccupee_(false)
    , pisteFermee_(false)
    , pisteOccupeePar_(-1)
    , totalAtterrissages_(0)
    , totalDecollages_(0)
    , totalCrashs_(0)
{
    // config piste centre ecran
    positionPiste_ = Position(400.0f, 300.0f);
 seuilPiste_ = Position(400.0f, 250.0f);
    pointApproche_ = Position(400.0f, 50.0f);
    pointDepart_ = Position(400.0f, 550.0f);

    // init parkings cote droit
    parkingOccupe_.resize(nombrePlacesParking, false);
    float departY = 150.0f;
    float espacement = 80.0f;
    
    for (int i = 0; i < nombrePlacesParking; ++i) {
   positionsParking_.push_back(Position(650.0f, departY + i * espacement));
    }

    Journaliseur::obtenirInstance().journaliser("TourControle", "systeme tour controle initialise avec " + 
        to_string(nombrePlacesParking) + " parkings", "INFO");
}

bool TourControle::demanderPiste(int idAvion, bool estUrgence) {
    lock_guard<mutex> verrou(mutexPiste_);
    
    // verif piste fermee
    if (pisteFermee_.load()) {
        stringstream ss;
    ss << "avion " << idAvion << " refuse piste fermee";
        Journaliseur::obtenirInstance().journaliser("TourControle", ss.str(), "WARN");
        return false;
    }
    
    // ajout file attente avec priorite
    fileAttentePiste_.push(DemandePiste(idAvion, estUrgence));
    
    stringstream ss;
    ss << "avion " << idAvion << " demande piste" 
       << (estUrgence ? " [URGENCE]" : "");
    Journaliseur::obtenirInstance().journaliser("TourControle", ss.str(), "INFO");
    
    // essayer accorder piste immediatement
    return essayerAccorderPiste(idAvion);
}

bool TourControle::essayerAccorderPiste(int idAvion) {
    if (pisteOccupee_.load() || fileAttentePiste_.empty()) {
        return false;
    }
    
    // verif si cet avion est le prochain
    DemandePiste prochaineDemande = fileAttentePiste_.top();
    if (prochaineDemande.idAvion != idAvion) {
      return false;
    }
    
    // accorder piste
    fileAttentePiste_.pop();
    pisteOccupee_ = true;
    pisteOccupeePar_ = idAvion;
    
    stringstream ss;
    ss << "avion " << idAvion << " piste accordee"
     << (prochaineDemande.estUrgence ? " [PRIORITE URGENCE]" : "");
    Journaliseur::obtenirInstance().journaliser("TourControle", ss.str(), "INFO");
    
    return true;
}

void TourControle::libererPiste(int idAvion) {
    lock_guard<mutex> verrou(mutexPiste_);
    
    if (pisteOccupee_.load() && pisteOccupeePar_ == idAvion) {
   pisteOccupee_ = false;
        pisteOccupeePar_ = -1;
    
      stringstream ss;
   ss << "avion " << idAvion << " libere piste";
        Journaliseur::obtenirInstance().journaliser("TourControle", ss.str(), "INFO");
        
        // notifier qu il y a des avions en attente
        if (!fileAttentePiste_.empty()) {
            DemandePiste prochaineDemande = fileAttentePiste_.top();
            stringstream ss2;
            ss2 << "piste libre avion suivant " << prochaineDemande.idAvion 
     << " peut redemander";
       Journaliseur::obtenirInstance().journaliser("TourControle", ss2.str(), "INFO");
        }
    }
}

bool TourControle::pisteOccupee() const {
    return pisteOccupee_.load();
}

void TourControle::fermerPiste() {
  lock_guard<mutex> verrou(mutexPiste_);
    pisteFermee_ = true;
    Journaliseur::obtenirInstance().journaliser("TourControle", "piste fermee urgence", "WARN");
}

void TourControle::ouvrirPiste() {
    lock_guard<mutex> verrou(mutexPiste_);
    pisteFermee_ = false;
    Journaliseur::obtenirInstance().journaliser("TourControle", "piste rouverte operations normales", "INFO");
}

bool TourControle::pisteFermee() const {
    return pisteFermee_.load();
}

int TourControle::obtenirAvionsEnAttente() const {
    lock_guard<mutex> verrou(mutexPiste_);
    return (int)fileAttentePiste_.size();
}

int TourControle::assignerParking(int idAvion) {
    lock_guard<mutex> verrou(mutexParking_);
  
    for (int i = 0; i < (int)parkingOccupe_.size(); ++i) {
     if (!parkingOccupe_[i]) {
       parkingOccupe_[i] = true;
 
            stringstream ss;
          ss << "avion " << idAvion << " assigne parking " << i;
        Journaliseur::obtenirInstance().journaliser("TourControle", ss.str(), "INFO");
            
     totalAtterrissages_++;
         return i;
        }
    }
    
    stringstream ss;
    ss << "pas de parking pour avion " << idAvion;
    Journaliseur::obtenirInstance().journaliser("TourControle", ss.str(), "ERROR");
    
    return -1;
}

void TourControle::libererParking(int idAvion, int idParking) {
    lock_guard<mutex> verrou(mutexParking_);
    
    if (idParking >= 0 && idParking < (int)parkingOccupe_.size()) {
        parkingOccupe_[idParking] = false;
      
      stringstream ss;
        ss << "avion " << idAvion << " libere parking " << idParking;
        Journaliseur::obtenirInstance().journaliser("TourControle", ss.str(), "INFO");
        
     totalDecollages_++;
    }
}

Position TourControle::obtenirPositionParking(int idParking) const {
    lock_guard<mutex> verrou(mutexParking_);
    
  if (idParking >= 0 && idParking < (int)positionsParking_.size()) {
     return positionsParking_[idParking];
    }
    
    return Position(0, 0);
}

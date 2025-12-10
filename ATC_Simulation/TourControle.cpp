#include "TourControle.hpp"
#include <sstream>

using namespace std;

TourControle::TourControle(int nombrePlacesParking)
    : pisteGaucheOccupee_(false)
    , pisteGaucheOccupeePar_(-1)
    , pisteDroiteOccupee_(false)
    , pisteDroiteOccupeePar_(-1)
    , pistesFermees_(false)
    , totalAtterrissages_(0)
    , totalDecollages_(0)
    , totalCrashs_(0)
{
    // config pistes VERTICALES
    // Piste GAUCHE (atterrissage) - x=350, dessin y=[50,550]
    positionPisteGauche_ = Position(350.0f, 300.0f);
    seuilPisteGauche_ = Position(350.0f, 50.0f);       // haut de la piste (debut dessin)
    finPisteGauche_ = Position(350.0f, 550.0f);        // bas de la piste (fin dessin)
    pointApproche_ = Position(350.0f, -50.0f);         // point approche en haut
    
    // Piste DROITE (decollage) - x=450, dessin y=[50,550]
    positionPisteDroite_ = Position(450.0f, 300.0f);
    seuilPisteDroite_ = Position(450.0f, 550.0f);      // debut decollage (bas de la piste)
    finPisteDroite_ = Position(450.0f, 50.0f);         // fin decollage (haut de la piste)
    pointDepart_ = Position(450.0f, -50.0f);           // point depart en haut
    
    // point croisement horizontal entre les deux pistes
    pointCroisement_ = Position(400.0f, 550.0f);       // en bas, entre les pistes

    // init parkings cote droit
    parkingOccupe_.resize(nombrePlacesParking, false);
  float departX = 650.0f;    // a droite des pistes
  float departY = 150.0f;
  float espacement = 80.0f;
    
    for (int i = 0; i < nombrePlacesParking; ++i) {
        positionsParking_.push_back(Position(departX, departY + i * espacement));  // alignes en bas
    }

    Journaliseur::obtenirInstance().journaliser("Tour Controle", 
        "systeme bi-piste VERTICALE initialise : piste gauche x=350 (atterrissage) + piste droite x=450 (decollage) avec " + 
        to_string(nombrePlacesParking) + " parkings", "INFO");
}

// ========== GESTION PISTE GAUCHE (ATTERRISSAGE) ==========

bool TourControle::demanderPisteGauche(int idAvion, bool estUrgence) {
    lock_guard<mutex> verrou(mutexPisteGauche_);
    
    if (pistesFermees_.load()) {
        return false;
    }
    
    // NOTE: on ne bloque plus les atterrissages en fonction de la file de decollage
    // cela creait des deadlocks. La gestion du flux se fait au niveau du parking.
    
    // verif si avion deja dans la file
    priority_queue<DemandePiste> fileCopie = fileAttentePisteGauche_;
    bool dejaEnAttente = false;
    
    while (!fileCopie.empty()) {
        if (fileCopie.top().idAvion == idAvion) {
            dejaEnAttente = true;
            break;
        }
        fileCopie.pop();
    }
    
    // ajout file attente seulement si pas deja present
    if (!dejaEnAttente) {
        fileAttentePisteGauche_.push(DemandePiste(idAvion, estUrgence));
        
        stringstream ss;
        ss << "avion " << idAvion << " demande piste GAUCHE (atterrissage)" 
        << (estUrgence ? " [URGENCE]" : "");
        Journaliseur::obtenirInstance().journaliser("Tour Controle", ss.str(), "INFO");
    }
    
    return essayerAccorderPisteGauche(idAvion);
}

bool TourControle::essayerAccorderPisteGauche(int idAvion) {
    if (pisteGaucheOccupee_.load() || fileAttentePisteGauche_.empty()) {
    return false;
    }
    
    // verif si cet avion est le prochain
    DemandePiste prochaineDemande = fileAttentePisteGauche_.top();
    if (prochaineDemande.idAvion != idAvion) {
        return false;
    }
    
    // accorder piste gauche
    fileAttentePisteGauche_.pop();
    pisteGaucheOccupee_ = true;
    pisteGaucheOccupeePar_ = idAvion;
    totalAtterrissages_++;
    
    stringstream ss;
    ss << "avion " << idAvion << " piste GAUCHE accordee (atterrissage)"
       << (prochaineDemande.estUrgence ? " [PRIORITE URGENCE]" : "");
    Journaliseur::obtenirInstance().journaliser("Tour Controle", ss.str(), "INFO");
    
    return true;
}

void TourControle::libererPisteGauche(int idAvion) {
    lock_guard<mutex> verrou(mutexPisteGauche_);
    
    if (pisteGaucheOccupee_.load() && pisteGaucheOccupeePar_ == idAvion) {
        pisteGaucheOccupee_ = false;
        pisteGaucheOccupeePar_ = -1;
        
    stringstream ss;
        ss << "avion " << idAvion << " libere piste GAUCHE";
 Journaliseur::obtenirInstance().journaliser("Tour Controle", ss.str(), "INFO");
     
        if (!fileAttentePisteGauche_.empty()) {
       DemandePiste prochaineDemande = fileAttentePisteGauche_.top();
            stringstream ss2;
         ss2 << "piste GAUCHE libre, avion suivant " << prochaineDemande.idAvion;
       Journaliseur::obtenirInstance().journaliser("Tour Controle", ss2.str(), "INFO");
        }
}
}

bool TourControle::pisteGaucheOccupee() const {
    return pisteGaucheOccupee_.load();
}

// ========== GESTION PISTE DROITE (DECOLLAGE) ==========

void TourControle::rejoindreFileDecollage(int idAvion) {
    lock_guard<mutex> verrou(mutexPisteDroite_);
    
    // verif si deja present
    for (int id : fileAttentePisteDroite_) {
        if (id == idAvion) return;
    }
    
    fileAttentePisteDroite_.push_back(idAvion);
    
    stringstream ss;
    ss << "avion " << idAvion << " rejoint file attente decollage (rang " 
       << fileAttentePisteDroite_.size() - 1 << ")";
    Journaliseur::obtenirInstance().journaliser("Tour Controle", ss.str(), "INFO");
}

int TourControle::obtenirRangDecollage(int idAvion) const {
    lock_guard<mutex> verrou(mutexPisteDroite_);
    
    for (size_t i = 0; i < fileAttentePisteDroite_.size(); ++i) {
        if (fileAttentePisteDroite_[i] == idAvion) {
            return (int)i;
        }
    }
    return -1;
}

bool TourControle::accorderDecollage(int idAvion) {
    lock_guard<mutex> verrou(mutexPisteDroite_);
    
    if (pistesFermees_.load()) return false;
    if (pisteDroiteOccupee_.load()) return false;
    if (fileAttentePisteDroite_.empty()) return false;
    
    // doit etre le premier
    if (fileAttentePisteDroite_.front() != idAvion) return false;
    
    // accorder
    fileAttentePisteDroite_.pop_front();
    pisteDroiteOccupee_ = true;
    pisteDroiteOccupeePar_ = idAvion;
    
    stringstream ss;
    ss << "avion " << idAvion << " piste DROITE accordee (decollage)";
    Journaliseur::obtenirInstance().journaliser("Tour Controle", ss.str(), "INFO");
    
    return true;
}

bool TourControle::demanderPisteDroite(int idAvion) {
    // methode compatibilite
    rejoindreFileDecollage(idAvion);
    return accorderDecollage(idAvion);
}

bool TourControle::essayerAccorderPisteDroite(int idAvion) {
    return accorderDecollage(idAvion);
}

void TourControle::libererPisteDroite(int idAvion) {
    lock_guard<mutex> verrou(mutexPisteDroite_);
    
    if (pisteDroiteOccupee_.load() && pisteDroiteOccupeePar_ == idAvion) {
        pisteDroiteOccupee_ = false;
 pisteDroiteOccupeePar_ = -1;
        totalDecollages_++;
        
        stringstream ss;
        ss << "avion " << idAvion << " libere piste DROITE";
        Journaliseur::obtenirInstance().journaliser("Tour Controle", ss.str(), "INFO");
        
     if (!fileAttentePisteDroite_.empty()) {
   int prochainId = fileAttentePisteDroite_.front();
            stringstream ss2;
          ss2 << "piste DROITE libre, avion suivant " << prochainId;
            Journaliseur::obtenirInstance().journaliser("Tour Controle", ss2.str(), "INFO");
  }
    }
}

bool TourControle::pisteDroiteOccupee() const {
    return pisteDroiteOccupee_.load();
}

// ========== FERMETURE/OUVERTURE PISTES ==========

void TourControle::fermerPistes() {
    pistesFermees_ = true;
    Journaliseur::obtenirInstance().journaliser("Tour Controle", "pistes FERMEES", "WARN");
}

void TourControle::ouvrirPistes() {
    pistesFermees_ = false;
  Journaliseur::obtenirInstance().journaliser("Tour Controle", "pistes OUVERTES", "INFO");
}

bool TourControle::pistesFermees() const {
    return pistesFermees_.load();
}

// ========== GESTION PARKINGS (INCHANGEE) ==========

int TourControle::assignerParking(int idAvion) {
    lock_guard<mutex> verrou(mutexParking_);
    
    for (int i = 0; i < (int)parkingOccupe_.size(); ++i) {
 if (!parkingOccupe_[i]) {
     parkingOccupe_[i] = true;
       
            stringstream ss;
            ss << "avion " << idAvion << " assigne parking " << i;
            Journaliseur::obtenirInstance().journaliser("Tour Controle", ss.str(), "INFO");
            
   return i;
        }
    }
    
    Journaliseur::obtenirInstance().journaliser("Tour Controle", 
        "aucun parking disponible pour avion " + to_string(idAvion), "WARN");
    return -1;
}

void TourControle::libererParking(int idAvion, int idParking) {
    lock_guard<mutex> verrou(mutexParking_);
    
    if (idParking >= 0 && idParking < (int)parkingOccupe_.size()) {
        parkingOccupe_[idParking] = false;
        
    stringstream ss;
 ss << "avion " << idAvion << " libere parking " << idParking;
        Journaliseur::obtenirInstance().journaliser("Tour Controle", ss.str(), "INFO");
    }
}

Position TourControle::obtenirPositionParking(int idParking) const {
    if (idParking >= 0 && idParking < (int)positionsParking_.size()) {
        return positionsParking_[idParking];
    }
    return Position(0.0f, 0.0f);
}

int TourControle::obtenirAvionsEnAttente() const {
    lock_guard<mutex> verrou(mutexPisteGauche_);
    return (int)fileAttentePisteGauche_.size();
}

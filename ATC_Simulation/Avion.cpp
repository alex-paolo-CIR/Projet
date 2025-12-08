#include "Avion.hpp"
#include <sstream>
#include <cmath>
#include <map>

using namespace std;

// definition constantes
const float Avion::TEMPS_TRAME = 0.016f;
const float Avion::TEMPS_PARKING = 8.0f;
const float Avion::INTERVALLE_VERIFICATION_ATTENTE = 0.5f;

Avion::Avion(int id, TourControle* tour, Position posDepart, float vitesse)
    : id_(id)
    , tour_(tour)
    , position_(posDepart)
    , positionCible_(posDepart)
    , angleRotation_(0.0f)
    , vitesse_(vitesse)
    , vitesseBase_(vitesse)
    , angleAttente_(0.0f)
    , etat_(EtatAvion::APPROCHE)
 , actif_(true)
    , enCours_(false)
    , carburant_(ConfigCarburant::CARBURANT_MAX)
    , aDemandePiste_(false)
    , parkingAssigne_(-1)
{
    stringstream ss;
    ss << "avion " << id_ << " cree position (" 
     << position_.x << ", " << position_.y << ") carburant " << carburant_.load();
    Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
}

Avion::~Avion() {
    arreter();
    if (thread_ && thread_->joinable()) {
        thread_->join();
    }
}

void Avion::demarrer() {
    enCours_ = true;
    thread_ = make_unique<thread>(&Avion::lancer, this);
  
    stringstream ss;
    ss << "avion " << id_ << " thread demarre";
  Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
}

void Avion::arreter() {
    enCours_ = false;
    actif_ = false;
}

void Avion::rejoindre() {
    if (thread_ && thread_->joinable()) {
        thread_->join();
  }
}

Position Avion::obtenirPosition() const {
  lock_guard<mutex> verrou(mutexPosition_);
    return position_;
}

float Avion::obtenirAngleRotation() const {
    lock_guard<mutex> verrou(mutexRotation_);
 return angleRotation_;
}

void Avion::marquerCommeEcrase() {
    mettreAJourEtat(EtatAvion::ECRASE);
    actif_ = false;
}

void Avion::lancer() {
    while (enCours_.load() && actif_.load()) {
   EtatAvion etatActuel = etat_.load();
    
 // maj carburant sauf parking et crash
 if (etatActuel != EtatAvion::STATIONNE && etatActuel != EtatAvion::ECRASE) {
            mettreAJourCarburant(TEMPS_TRAME);
        }
        
        // verif carburant prioritaire
        verifierStatutCarburant();

        // si crash arreter immediatement
        if (etat_.load() == EtatAvion::ECRASE) {
            phaseEcrase();
   break;
    }
   
        // machine a etats
        switch (etatActuel) {
            case EtatAvion::APPROCHE:
     phaseApproche();
      break;
          case EtatAvion::ATTENTE:
                phaseAttente();
          break;
case EtatAvion::ATTERRISSAGE:
   phaseAtterrissage();
      break;
            case EtatAvion::ATTENTE_CROISEMENT:
                phaseAttenteCroisement();
  break;
            case EtatAvion::CROISEMENT:
     phaseCroisement();
      break;
        case EtatAvion::ROULAGE_ENTREE:
      phaseRoulageEntree();
           break;
     case EtatAvion::STATIONNE:
           phaseStationnement();
     break;
            case EtatAvion::ROULAGE_SORTIE:
          phaseRoulageSortie();
              break;
            case EtatAvion::DECOLLAGE:
  phaseDecollage();
                break;
       case EtatAvion::DEPART:
    phaseDepart();
                break;
          case EtatAvion::URGENCE:
      phaseUrgence();
              break;
            case EtatAvion::PARTI:
                actif_ = false;
       break;
   default:
     break;
        }
 
        // seul sleep boucle physique 60 fps
        this_thread::sleep_for(chrono::milliseconds((int)(TEMPS_TRAME * 1000)));
    }
    
    stringstream ss;
    ss << "avion " << id_ << " thread terme";
    Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
}

void Avion::mettreAJourCarburant(float dt) {
    float carburantActuel = carburant_.load();
    carburantActuel -= ConfigCarburant::TAUX_CONSOMMATION_CARBURANT * dt;
    
    if (carburantActuel < 0.0f) {
        carburantActuel = 0.0f;
    }
    
    carburant_ = carburantActuel;
}

void Avion::ravitaillerAuParking(float dt) {
    float carburantActuel = carburant_.load();
    carburantActuel += ConfigCarburant::TAUX_REMPLISSAGE_CARBURANT * dt;
    
    if (carburantActuel > ConfigCarburant::CARBURANT_MAX) {
        carburantActuel = ConfigCarburant::CARBURANT_MAX;
  }
    
    carburant_ = carburantActuel;
}

void Avion::verifierStatutCarburant() {
    float carburantActuel = carburant_.load();
    EtatAvion etatActuel = etat_.load();
    
    // crash immediat si plus de carburant
    if (carburantActuel <= 0.0f && etatActuel != EtatAvion::ECRASE) {
     stringstream ss;
        ss << "avion " << id_ << " crash plus de carburant position (" 
        << position_.x << ", " << position_.y << ")";
        Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "ERROR");
        marquerCommeEcrase();
     return;
    }
    
    // urgence si carburant bas
    if (carburantActuel < ConfigCarburant::SEUIL_URGENCE_CARBURANT && 
        etatActuel != EtatAvion::URGENCE &&
        etatActuel != EtatAvion::ECRASE &&
        etatActuel != EtatAvion::STATIONNE) {
  
        stringstream ss;
   ss << "avion " << id_ << " urgence carburant bas " << carburantActuel;
        Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "WARN");
        mettreAJourEtat(EtatAvion::URGENCE);
    }
}
void Avion::phaseApproche() {
    bool doitCreerTrajectoire = false;

    {
        lock_guard<mutex> verrou(mutexWaypoints_);
        if (waypoints_.empty()) {
            doitCreerTrajectoire = true;
     }
    }

    if (doitCreerTrajectoire) {
        Position depart = obtenirPosition();
        Position arrivee = tour_->obtenirPointApproche();
    ajouterTrajectoire(depart, arrivee);

        stringstream ss;
        ss << "avion " << id_ << " approche rectiligne de ("
       << depart.x << "," << depart.y << ") vers ("
            << arrivee.x << "," << arrivee.y << ")";
        Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
    }

    mettreAJourWaypoints(TEMPS_TRAME);

    Position pointApproche = tour_->obtenirPointApproche();
    if (aCibleAtteinte(pointApproche, 10.0f)) {
        viderWaypoints();

  bool estUrgence = carburant_.load() < ConfigCarburant::SEUIL_URGENCE_CARBURANT;
        if (tour_->demanderPisteGauche(id_, estUrgence)) {
      mettreAJourEtat(EtatAvion::ATTERRISSAGE);
            aDemandePiste_ = true;

            Position posActuelle = obtenirPosition();
        ajouterTrajectoire(posActuelle, tour_->obtenirSeuilPisteGauche());
        }
        else {
    mettreAJourEtat(EtatAvion::ATTENTE);
 aDemandePiste_ = false;
        }
  }
}

void Avion::phaseAttente() {
    if (carburant_.load() < ConfigCarburant::SEUIL_URGENCE_CARBURANT && 
        etat_.load() != EtatAvion::URGENCE) {
        mettreAJourEtat(EtatAvion::URGENCE);
    aDemandePiste_ = false;
return;
    }
    
    Position centreOvale(200.0f, 200.0f);
float rayonX = 100.0f;
    float rayonY = 120.0f;

    angleAttente_ += 3.0f;
    if (angleAttente_ >= 360.0f) {
  angleAttente_ -= 360.0f;
    }
    
    float angleRad = angleAttente_ * 3.14159f / 180.0f;
    Position nouvellePos;
    nouvellePos.x = centreOvale.x + cos(angleRad) * rayonX;
    nouvellePos.y = centreOvale.y + sin(angleRad) * rayonY;
    
    positionCible_ = nouvellePos;
    deplacerVers(positionCible_, TEMPS_TRAME);

    if (angleAttente_ < 5.0f) {
        bool estUrgence = carburant_.load() < ConfigCarburant::SEUIL_URGENCE_CARBURANT;
        
        if (tour_->demanderPisteGauche(id_, estUrgence)) {
        mettreAJourEtat(EtatAvion::ATTERRISSAGE);
            aDemandePiste_ = true;
    vitesse_ = vitesseBase_ * 1.2f;
      
         Position posActuelle = obtenirPosition();
 ajouterTrajectoire(posActuelle, tour_->obtenirSeuilPisteGauche());
       
         stringstream ss;
            ss << "avion " << id_ << " quitte attente, atterrissage autorise";
 Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
        }
  }
}
void Avion::phaseUrgence() {
    Position centreOvale(200.0f, 200.0f);
 float rayonX = 80.0f;  // plus serre
    float rayonY = 100.0f;
    
    // tourner plus vite en urgence
    angleAttente_ += 4.0f;
    if (angleAttente_ >= 360.0f) {
        angleAttente_ -= 360.0f;
    }
    
    // calculer position sur ovale
    float angleRad = angleAttente_ * 3.14159f / 180.0f;
    Position nouvellePos;
    nouvellePos.x = centreOvale.x + cos(angleRad) * rayonX;
    nouvellePos.y = centreOvale.y + sin(angleRad) * rayonY;
    
    // deplacer vers cette position
    positionCible_ = nouvellePos;
    deplacerVers(positionCible_, TEMPS_TRAME);
    
 if (angleAttente_ < 5.0f || (angleAttente_ > 175.0f && angleAttente_ < 185.0f)) {
        if (tour_->demanderPisteGauche(id_, true)) {
     mettreAJourEtat(EtatAvion::ATTERRISSAGE);
      aDemandePiste_ = true;
  vitesse_ = vitesseBase_ * 1.5f;
 
         Position posActuelle = obtenirPosition();
    ajouterTrajectoire(posActuelle, tour_->obtenirSeuilPisteGauche());
      
       stringstream ss;
     ss << "avion " << id_ << " urgence terminee, atterrissage prioritaire autorise";
  Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "WARN");
        }
    }
}

void Avion::phaseAtterrissage() {
    // Augmenter vitesse sur piste
    vitesse_ = vitesseBase_ * 2.0f;

    
    {
        lock_guard<mutex> verrou(mutexWaypoints_);
        if (waypoints_.empty()) {
            Position depart = obtenirPosition();
            Position arrivee = tour_->obtenirFinPisteGauche();

            // Libérer mutex avant d'appeler ajouterTrajectoire (qui le verrouille)
            mutexWaypoints_.unlock();
            ajouterTrajectoire(depart, arrivee);
            mutexWaypoints_.lock();

            stringstream ss;
            ss << "avion " << id_ << " atterrissage rectiligne de ("
                << depart.x << "," << depart.y << ") vers ("
                << arrivee.x << "," << arrivee.y << ")";
            Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
        }
    }

    // Suivre waypoints définis
    mettreAJourWaypoints(TEMPS_TRAME);

    Position finPiste = tour_->obtenirFinPisteGauche();

    // Si arrivé au bout de la piste
    if (aCibleAtteinte(finPiste, 10.0f)) {
        viderWaypoints();

        // Libérer piste gauche
        if (aDemandePiste_) {
            tour_->libererPisteGauche(id_);
            aDemandePiste_ = false;

            stringstream ss;
            ss << "avion " << id_ << " a termine atterrissage, libere piste gauche";
            Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
        }

        // Passer en attente croisement
        mettreAJourEtat(EtatAvion::ATTENTE_CROISEMENT);
    }
}

void Avion::phaseAttenteCroisement() {
    // avion ARRETE au bout de la piste gauche
    // attend que la piste DROITE soit libre pour traverser
    vitesse_ = 0.0f;  // ARRETER NET
    
    // verifier si piste droite libre
    if (!tour_->pisteDroiteOccupee()) {
 // piste droite libre! demander autorisation
  if (tour_->demanderPisteDroite(id_)) {
            // autorisation accordee transition vers croisement
            mettreAJourEtat(EtatAvion::CROISEMENT);
     positionCible_ = tour_->obtenirPointCroisement();
   vitesse_ = vitesseBase_ * 1.0f;  // vitesse normale pour traverser
            aDemandePiste_ = true;
        
            stringstream ss;
 ss << "avion " << id_ << " traverse vers parking (piste droite libre)";
    Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
        }
    }
    // sinon reste immobile et attend
}

void Avion::phaseCroisement() {
    // Définir trajectoire RECTILIGNE une seule fois
    bool doitCreerTrajectoire = false;

    {
        lock_guard<mutex> verrou(mutexWaypoints_);
        if (waypoints_.empty()) {
            doitCreerTrajectoire = true;
        }
    }

    if (doitCreerTrajectoire) {
        Position depart = obtenirPosition();
        Position arrivee = tour_->obtenirPointCroisement();

        ajouterTrajectoire(depart, arrivee);

        stringstream ss;
        ss << "avion " << id_ << " croisement rectiligne de ("
            << depart.x << "," << depart.y << ") vers ("
            << arrivee.x << "," << arrivee.y << ")";
        Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
    }

    // Suivre waypoints
    mettreAJourWaypoints(TEMPS_TRAME);

    Position pointCroisement = tour_->obtenirPointCroisement();
    if (aCibleAtteinte(pointCroisement, 10.0f)) {
        viderWaypoints();

        // Libérer piste droite
        if (aDemandePiste_) {
            tour_->libererPisteDroite(id_);
            aDemandePiste_ = false;
        }

        // Demander parking
        parkingAssigne_ = tour_->assignerParking(id_);

        if (parkingAssigne_ >= 0) {
            mettreAJourEtat(EtatAvion::ROULAGE_ENTREE);
            Position posActuelle = obtenirPosition();
            ajouterTrajectoire(posActuelle, tour_->obtenirPositionParking(parkingAssigne_));
            vitesse_ = vitesseBase_ * 1.5f;
        }
        else {
            mettreAJourEtat(EtatAvion::ROULAGE_SORTIE);
            Position posActuelle = obtenirPosition();
            ajouterTrajectoire(posActuelle, tour_->obtenirSeuilPisteDroite());
        }
    }
}

void Avion::phaseRoulageEntree() {
    // *** CORRECTION : Utiliser système de waypoints ***

    // Définir trajectoire une seule fois
    bool doitCreerTrajectoire = false;

    {
        lock_guard<mutex> verrou(mutexWaypoints_);
        if (waypoints_.empty()) {
            doitCreerTrajectoire = true;
        }
    }

    if (doitCreerTrajectoire && parkingAssigne_ >= 0) {
        Position depart = obtenirPosition();
        Position arrivee = tour_->obtenirPositionParking(parkingAssigne_);

        ajouterTrajectoire(depart, arrivee);

        stringstream ss;
        ss << "avion " << id_ << " roulage vers parking " << parkingAssigne_
            << " de (" << depart.x << "," << depart.y << ") vers ("
            << arrivee.x << "," << arrivee.y << ")";
        Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
    }

    // Vitesse accélérée
    vitesse_ = vitesseBase_ * 1.5f;

    // Suivre waypoints
    mettreAJourWaypoints(TEMPS_TRAME);

    // Vérifier arrivée parking
    if (parkingAssigne_ >= 0) {
        Position posParking = tour_->obtenirPositionParking(parkingAssigne_);
        if (aCibleAtteinte(posParking, 10.0f)) {
            viderWaypoints();
            mettreAJourEtat(EtatAvion::STATIONNE);

            stringstream ss;
            ss << "avion " << id_ << " arrive parking " << parkingAssigne_
                << " ravitaillement carburant " << carburant_.load();
            Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
        }
    }
}

void Avion::phaseStationnement() {
    // ravitaillement progressif accelere
    ravitaillerAuParking(TEMPS_TRAME * 2.0f);
    
    static map<int, chrono::steady_clock::time_point> minuteursParking;
    
    if (minuteursParking.find(id_) == minuteursParking.end()) {
        minuteursParking[id_] = chrono::steady_clock::now();
    }
    
    long long ecoule = chrono::duration_cast<chrono::seconds>(
        chrono::steady_clock::now() - minuteursParking[id_]
    ).count();
    
    if (carburant_.load() >= ConfigCarburant::CARBURANT_MAX * 0.95f && ecoule >= (long long)TEMPS_PARKING) {
  
        if (tour_->pisteDroiteOccupee()) {
    stringstream ss;
  ss << "avion " << id_ << " pret a partir mais attend fin decollage autre avion";
       Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
    return;
  }
     
   stringstream ss;
        ss << "avion " << id_ << " ravitaille " << carburant_.load() 
           << " apres " << ecoule << " sec - pret au depart";
     Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
        
        tour_->libererParking(id_, parkingAssigne_);
        
        mettreAJourEtat(EtatAvion::ROULAGE_SORTIE);
  positionCible_ = tour_->obtenirSeuilPisteDroite();
        
        minuteursParking.erase(id_);
      aDemandePiste_ = false;
    }
}
void Avion::phaseRoulageSortie() {
    // *** CORRECTION : Utiliser système de waypoints ***

    // Définir trajectoire une seule fois
    bool doitCreerTrajectoire = false;

    {
        lock_guard<mutex> verrou(mutexWaypoints_);
        if (waypoints_.empty()) {
            doitCreerTrajectoire = true;
        }
    }

    if (doitCreerTrajectoire) {
        Position depart = obtenirPosition();
        Position arrivee = tour_->obtenirSeuilPisteDroite();

        ajouterTrajectoire(depart, arrivee);

        stringstream ss;
        ss << "avion " << id_ << " roulage sortie vers piste droite de ("
            << depart.x << "," << depart.y << ") vers ("
            << arrivee.x << "," << arrivee.y << ")";
        Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
    }

    // Vitesse accélérée
    vitesse_ = vitesseBase_ * 1.5f;

    // Suivre waypoints
    mettreAJourWaypoints(TEMPS_TRAME);

    // Vérifier arrivée seuil piste droite
    Position seuilPiste = tour_->obtenirSeuilPisteDroite();
    if (aCibleAtteinte(seuilPiste, 10.0f)) {
        viderWaypoints();

        // Demander autorisation décollage
        if (!aDemandePiste_) {
            if (tour_->demanderPisteDroite(id_)) {
                // Autorisation accordée
                mettreAJourEtat(EtatAvion::DECOLLAGE);
                aDemandePiste_ = true;

                // Définir trajectoire décollage
                Position posActuelle = obtenirPosition();
                ajouterTrajectoire(posActuelle, tour_->obtenirPointDepart());

                stringstream ss;
                ss << "avion " << id_ << " autorisation decollage accordee";
                Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
            }
        }
    }
}

void Avion::phaseDecollage() {
    

    // Définir trajectoire une seule fois
    bool doitCreerTrajectoire = false;

    {
        lock_guard<mutex> verrou(mutexWaypoints_);
        if (waypoints_.empty()) {
            doitCreerTrajectoire = true;
        }
    }

    if (doitCreerTrajectoire) {
        Position depart = obtenirPosition();
        Position arrivee = tour_->obtenirPointDepart();

        ajouterTrajectoire(depart, arrivee);

        stringstream ss;
        ss << "avion " << id_ << " decollage de ("
            << depart.x << "," << depart.y << ") vers ("
            << arrivee.x << "," << arrivee.y << ")";
        Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
    }

    // Augmenter vitesse sur piste DROITE
    vitesse_ = vitesseBase_ * 2.0f;

    // Suivre waypoints
    mettreAJourWaypoints(TEMPS_TRAME);

    // Vérifier fin décollage
    Position pointDepart = tour_->obtenirPointDepart();
    if (aCibleAtteinte(pointDepart, 10.0f)) {
        viderWaypoints();

        // Libérer piste DROITE immédiatement
        tour_->libererPisteDroite(id_);
        aDemandePiste_ = false;

        // Réinitialiser vitesse normale
        vitesse_ = vitesseBase_;

        // Continuer vers le haut pour sortir de l'écran
        mettreAJourEtat(EtatAvion::DEPART);

        // Définir point de sortie très haut
        Position posActuelle = obtenirPosition();
        Position sortieEcran(posActuelle.x, -200.0f);  // Sortir par le haut
        ajouterTrajectoire(posActuelle, sortieEcran);

        stringstream ss;
        ss << "avion " << id_ << " decollage termine, en depart";
        Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
    }
}

void Avion::phaseDepart() {
    // *** CORRECTION : Utiliser système de waypoints ***

    // Définir trajectoire sortie une seule fois
    bool doitCreerTrajectoire = false;

    {
        lock_guard<mutex> verrou(mutexWaypoints_);
        if (waypoints_.empty()) {
            doitCreerTrajectoire = true;
        }
    }

    if (doitCreerTrajectoire) {
        Position depart = obtenirPosition();
        Position sortieHaut(depart.x, -200.0f);  // Sortir par le haut

        ajouterTrajectoire(depart, sortieHaut);

        stringstream ss;
        ss << "avion " << id_ << " depart vers sortie ecran";
        Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
    }

    // Suivre waypoints
    mettreAJourWaypoints(TEMPS_TRAME);

    // Vérifier si hors écran
    Position posActuelle = obtenirPosition();
    if (posActuelle.y < -150.0f || posActuelle.y > 750.0f ||
        posActuelle.x < -150.0f || posActuelle.x > 950.0f) {

        viderWaypoints();
        mettreAJourEtat(EtatAvion::PARTI);

        stringstream ss;
        ss << "avion " << id_ << " a quitte l'espace aerien";
        Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
    }
}
void Avion::deplacerVers(const Position& cible, float dt) {
    // ne pas bouger si crash
    if (etat_.load() == EtatAvion::ECRASE) {
  return;
    }
    
    lock_guard<mutex> verrou(mutexPosition_);
    
    float dx = cible.x - position_.x;
 float dy = cible.y - position_.y;
    float distance = sqrt(dx * dx + dy * dy);
    
    if (distance > 0.1f) {
        float distanceDeplacement = vitesse_ * dt;
        if (distanceDeplacement > distance) {
        distanceDeplacement = distance;
     }
    
position_.x += (dx / distance) * distanceDeplacement;
        position_.y += (dy / distance) * distanceDeplacement;
        
        // maj angle rotation
        {
        lock_guard<mutex> verrouRot(mutexRotation_);
         angleRotation_ = atan2(dy, dx) * 180.0f / 3.14159f;
        }
    }
}

void Avion::mettreAJourEtat(EtatAvion nouvelEtat) {
    EtatAvion ancienEtat = etat_.load();
    etat_ = nouvelEtat;
    
    stringstream ss;
 ss << "avion " << id_ << " changement etat " 
       << EtatVersTexte(ancienEtat) << " vers " << EtatVersTexte(nouvelEtat);
    Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
}

bool Avion::aCibleAtteinte(const Position& cible, float seuil) const {
    Position posActuelle = obtenirPosition();
    return posActuelle.distanceVers(cible) < seuil;
}


// définition des waypoints

void Avion::ajouterWaypoint(const Position& waypoint) {
    lock_guard<mutex> verrou(mutexWaypoints_);
    waypoints_.push(waypoint);
}

void Avion::ajouterTrajectoire(const Position& depart, const Position& arrivee) {
    lock_guard<mutex> verrou(mutexWaypoints_);

  while (!waypoints_.empty()) {
        waypoints_.pop();
    }

    float dx = arrivee.x - depart.x;
    float dy = arrivee.y - depart.y;
    
    bool horizontalDabord = abs(dx) > abs(dy);
    
    Position coin;
    if (horizontalDabord) {
    coin.x = arrivee.x;
        coin.y = depart.y;
    } else {
        coin.x = depart.x;
        coin.y = arrivee.y;
    }
    
    float distance1 = depart.distanceVers(coin);
    int nombreWaypoints1 = static_cast<int>(distance1 / 50.0f);
    if (nombreWaypoints1 < 1) nombreWaypoints1 = 1;
    
    for (int i = 1; i <= nombreWaypoints1; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(nombreWaypoints1);
        Position waypoint;
  waypoint.x = depart.x + (coin.x - depart.x) * t;
waypoint.y = depart.y + (coin.y - depart.y) * t;
        waypoints_.push(waypoint);
    }
    
    float distance2 = coin.distanceVers(arrivee);
    int nombreWaypoints2 = static_cast<int>(distance2 / 50.0f);
    if (nombreWaypoints2 < 1) nombreWaypoints2 = 1;
    
    for (int i = 1; i <= nombreWaypoints2; ++i) {
      float t = static_cast<float>(i) / static_cast<float>(nombreWaypoints2);
        Position waypoint;
        waypoint.x = coin.x + (arrivee.x - coin.x) * t;
        waypoint.y = coin.y + (arrivee.y - coin.y) * t;
     waypoints_.push(waypoint);
    }

    stringstream ss;
    ss << "avion " << id_ << " trajectoire en L definie " 
       << (nombreWaypoints1 + nombreWaypoints2) << " waypoints "
 << "(coin " << coin.x << "," << coin.y << ")";
  Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "DEBUG");
}


void Avion::viderWaypoints() {
    lock_guard<mutex> verrou(mutexWaypoints_);
    while (!waypoints_.empty()) {
        waypoints_.pop();
    }
}

void Avion::mettreAJourWaypoints(float dt) {
    Position waypointActuel;
  bool aWaypoint = false;
    bool waypointAtteint = false;

    {
        lock_guard<mutex> verrou(mutexWaypoints_);

        if (!waypoints_.empty()) {
    waypointActuel = waypoints_.front();
            aWaypoint = true;
 }
    }

    if (!aWaypoint) {
        return;
    }

    deplacerVers(waypointActuel, dt);

    if (aCibleAtteinte(waypointActuel, 3.0f)) {
        lock_guard<mutex> verrou(mutexWaypoints_);

        if (!waypoints_.empty()) {
  waypoints_.pop();
        waypointAtteint = true;

       if (!waypoints_.empty()) {
            stringstream ss;
        ss << "avion " << id_ << " waypoint atteint, reste " << waypoints_.size();
   Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "DEBUG");
 }
       else {
   stringstream ss;
     ss << "avion " << id_ << " dernier waypoint atteint";
    Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
     }
        }
    }
}

void Avion::phaseEcrase() {
    // avion reste immobile
    stringstream ss;
    ss << "avion " << id_ << " reste ecrase position ("
        << position_.x << ", " << position_.y << ") bloque piste";
    Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "ERROR");

    // rester actif pour affichage mais arreter thread
}
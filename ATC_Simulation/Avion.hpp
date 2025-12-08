#pragma once
#include "Commun.hpp"
#include "TourControle.hpp"
#include "Journaliseur.hpp"
#include <SFML/Graphics.hpp>
#include <thread>
#include <atomic>
#include <memory>
#include <chrono>

using namespace std;

// avion autonome
class Avion {
public:
    Avion(int id, TourControle* tour, Position posDepart, float vitesse = 50.0f);
    ~Avion();

    // demarrer thread
    void demarrer();
    
    // arreter thread
    void arreter();
    
    // attendre fin thread
 void rejoindre();

    // accesseurs visualisation
    int obtenirId() const { return id_; }
    Position obtenirPosition() const;
    EtatAvion obtenirEtat() const { return etat_.load(); }
  float obtenirVitesse() const { return vitesse_; }
    bool estActif() const { return actif_.load(); }
    
    // carburant
  float obtenirCarburant() const { return carburant_.load(); }
    float obtenirPourcentageCarburant() const { return (carburant_.load() / ConfigCarburant::CARBURANT_MAX) * 100.0f; }
    bool estEnUrgence() const { return carburant_.load() < ConfigCarburant::SEUIL_URGENCE_CARBURANT; }
    
    // setter carburant pour tests
    void definirCarburant(float quantite) { carburant_ = quantite; }
    
    // angle rotation
    float obtenirAngleRotation() const;

    // marquer crash
    void marquerCommeEcrase();

private:
    // methode principale thread
    void lancer();
    
    // phases cycle de vie
    void phaseApproche();
    void phaseAttente();
    void phaseAtterrissage();
    void phaseAttenteCroisement();  // nouvelle phase : attend de traverser
    void phaseCroisement();          // nouvelle phase : traverse la piste droite
    void phaseRoulageEntree();
    void phaseStationnement();
    void phaseRoulageSortie();
    void phaseDecollage();
    void phaseDepart();
    void phaseUrgence();
    void phaseEcrase();

    // utilitaires
    void deplacerVers(const Position& cible, float dt);
    void mettreAJourEtat(EtatAvion nouvelEtat);
    bool aCibleAtteinte(const Position& cible, float seuil = 5.0f) const;

	// systeme de waypoints 
	void ajouterWaypoint(const Position& waypoint);
    void ajouterTrajectoire(const Position& depart, const Position& arrivee);
    void mettreAJourWaypoints(float dt);
    void viderWaypoints();

    // file de waypoints
	queue<Position> waypoints_;
	mutable mutex mutexWaypoints_;
    
    // gestion carburant
    void mettreAJourCarburant(float dt);
    void ravitaillerAuParking(float dt);
    void verifierStatutCarburant();

    // attributs
    int id_;
    TourControle* tour_;
    
    mutable mutex mutexPosition_;
    Position position_;
    Position positionCible_;
    mutable mutex mutexRotation_;
    float angleRotation_;
    
    float vitesse_;
    float vitesseBase_;
    float angleAttente_;
    atomic<EtatAvion> etat_;
    atomic<bool> actif_;
    atomic<bool> enCours_;
    atomic<float> carburant_;
    bool aDemandePiste_;
    
    int parkingAssigne_;
    unique_ptr<thread> thread_;
  
    // constantes simulation
    static const float TEMPS_TRAME;
    static const float TEMPS_PARKING;
    static const float INTERVALLE_VERIFICATION_ATTENTE;
};

/*
 * i18n.c: Internationalization
 *
 * See the README file for copyright information and how to reach the author.
 * Russian: Vyacheslav Dikonov <sdiconov@mail.ru>
 *
 * $Id: 
 */

#include "i18n.h"

const tI18nPhrase Phrases[] = {
  { "Drive East",
    "Nach Osten",
    "",// TODO
    "Muove a Est",
    "",// TODO
    "",// TODO
    "Aller Est",
    "",// TODO
    "",// TODO
    "",// TODO
    "Mueve a Este",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Mou cap a l'est",
    "+� ������",
    "",// TODO
    "",// TODO
    "",// TODO
  },
  { "Halt",
    "Anhalten",
    "",// TODO
    "Ferma",
    "",// TODO
    "",// TODO
    "Arr�t",
    "",// TODO
    "",// TODO
    "",// TODO
    "Para",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Atura",
    "����",
    "",// TODO
    "",// TODO
    "",// TODO
  },
  { "Drive West",
    "Nach Westen",
    "",// TODO
    "Muove a Ovest",
    "",// TODO
    "",// TODO
    "Aller Ouest",
    "",// TODO
    "",// TODO
    "",// TODO
    "Mueve a Oeste",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Mou cap a l'oest",
    "+� �����",
    "",// TODO
    "",// TODO
    "",// TODO
  },
  { "Recalc",
    "Recalc",
    "",// TODO
    "Ricalcola",
    "",// TODO
    "",// TODO
    "Recalc",
    "",// TODO
    "",// TODO
    "",// TODO
    "Recalcula",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Recalcula",
    "�����������",
    "",// TODO
    "",// TODO
    "",// TODO
  },
  { "Goto %d",
    "Gehe zu %d",
    "",// TODO
    "Va a %d",
    "",// TODO
    "",// TODO
    "Aller � %d",
    "",// TODO
    "",// TODO
    "",// TODO
    "Ir a %d",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Anar a %d",
    "������� %d",
    "",// TODO
    "",// TODO
    "",// TODO
  },
  { "Store",
    "Speichere",
    "",// TODO
    "Memorizza",
    "",// TODO
    "",// TODO
    "Enregistrer",
    "",// TODO
    "",// TODO
    "",// TODO
    "Memoriza",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Memoritza",
    "T��������",
    "",// TODO
    "",// TODO
    "",// TODO
  },
  { "%d Steps East",
    "%d Schritte Osten",
    "",// TODO
    "%d Passi Est",
    "",// TODO
    "",// TODO
    "%d Incr�ment Est",
    "",// TODO
    "",// TODO
    "",// TODO
    "%d Pasos Este",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "%d Passos Est",
    "%d ���(��) �� ������",
    "",// TODO
    "",// TODO
    "",// TODO
  },
  { "%d Steps West",
    "%d Schritte Westen",
    "",// TODO
    "%d Passi Ovest",
    "",// TODO
    "",// TODO
    "%d Incr�ment Ouest",
    "",// TODO
    "",// TODO
    "",// TODO
    "%d Pasos Oeste",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "%d Passos Oest",
    "���(��) �� �����",
    "",// TODO
    "",// TODO
    "",// TODO
  },
  { "Set East Limit",
    "Ost-Limit setzen",
    "",// TODO
    "Imposta Limite Est",
    "",// TODO
    "",// TODO
    "Fixer Limite Est",
    "",// TODO
    "",// TODO
    "",// TODO
    "Ajusta Limite Este",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Ajusta L�mit Est",
    "��������� ������",
    "",// TODO
    "",// TODO
    "",// TODO
  },
  { "Set Zero",
    "",// TODO
    "",// TODO
    "Messa a zero",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Puesta a cero",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Posada a zero",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
  },
  { "Set West Limit",
    "West-Limit setzen",
    "",// TODO
    "Imposta Limite Ovest",// TODO
    "",// TODO
    "",// TODO
    "Fixer Limite Ouest",
    "",// TODO
    "",// TODO
    "",// TODO
    "Ajusta Limite Oeste",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Ajusta L�mit Oest",
    "T������� ������",
    "",// TODO
    "",// TODO
    "",// TODO
  },
  { "Frequency:",
    "Frequenz:",
    "",// TODO
    "Frequenza:",
    "",// TODO
    "",// TODO
    "Fr�quence:",
    "",// TODO
    "",// TODO
    "",// TODO
    "Frecuencia:",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Freq��ncia:",
    "�������:",
    "",// TODO
    "",// TODO
    "",// TODO
  },
  { "Symbolrate:",
    "Symbolrate:",
    "",// TODO
    "Symbolrate:",
    "",// TODO
    "",// TODO
    "SymbolRate:",
    "",// TODO
    "",// TODO
    "",// TODO
    "SymbolRate:",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "SymbolRate:",
    "����. ��������",
    "",// TODO
    "",// TODO
    "",// TODO
  },
  { "Vpid:",
    "Vpid:",
    "",// TODO
    "Vpid:",
    "",// TODO
    "",// TODO
    "Vpid:",
    "",// TODO
    "",// TODO
    "",// TODO
    "Vpid:",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Vpid:",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
  },
  { "Apid:",
    "Apid:",
    "",// TODO
    "Apid:",
    "",// TODO
    "",// TODO
    "Apid:",
    "",// TODO
    "",// TODO
    "",// TODO
    "Apid:",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Apid:",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
  },
  { "Scan Transponder",
    "Scan Transponder",
    "",// TODO
    "Scansione Transponder",
    "",// TODO
    "",// TODO
    "Scanner Transpondeur",
    "",// TODO
    "",// TODO
    "",// TODO
    "Escaneo Transponder",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Escaneja Transponder",
    "����. ��������",
    "",// TODO
    "",// TODO
    "",// TODO
  },
  { "Enable Limits",
    "Limits ein",
    "",// TODO
    "Attiva Limiti",
    "",// TODO
    "",// TODO
    "Activer Limites",
    "",// TODO
    "",// TODO
    "",// TODO
    "Activa Limites",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Activa L�mits",
    "�������� �������",
    "",// TODO
    "",// TODO
    "",// TODO
  },
  { "Disable Limits",
    "Limits aus",
    "",// TODO
    "Disattiva Limiti",
    "",// TODO
    "",// TODO
    "D�sactiver Limites",
    "",// TODO
    "",// TODO
    "",// TODO
    "Desactiva Limites",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Desactiva L�mits",
    "+�������� �������",
    "",// TODO
    "",// TODO
    "",// TODO
  },
  { "Dish target: %d, position: %d",
    "",// TODO
    "",// TODO
    "Obbiettivo: %d, posizione: %d",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Destino disco: %d, posici�n: %d",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Destinaci� disc: %d, posici�: %d",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
  },
  { "Position reached",
    "",// TODO
    "",// TODO
    "Posizione raggiunta",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Posici�n alcanzada",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Arribat en posici�",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
  },
  { "Motor wait",
    "",// TODO
    "",// TODO
    "Attesa motore",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Espera motor",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Espera motor",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
  },
  { "Motor error",
    "",// TODO
    "",// TODO
    "Errore motore",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Error motor",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Error motor",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
  },
  { "Position not set",
    "",// TODO
    "",// TODO
    "Posizione non impostata",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Posici�n no ajustada",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Posici� no ajustada",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
  },
  { "Not positioned",
    "",// TODO
    "",// TODO
    "Non posizionato",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "No posicionado",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "No posicionat",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
  },
  { "Dish at limits",
    "",// TODO
    "",// TODO
    "Raggiunto limite",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Limite alcanzado",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Arribat a l�mit",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
  },
  { "Position outside limits",
    "",// TODO
    "",// TODO
    "Posizione fuori dai limiti",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Fuera de los limites",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Posici� fora dels l�mits",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
  },
//Setup
  { "Card, connected with motor",
    "Karte, die mit Motor verbunden ist ",
    "",// TODO
    "Scheda collegata al motore",
    "",// TODO
    "",// TODO
    "Carte, connect�e au moteur",
    "",// TODO
    "",// TODO
    "",// TODO
    "Tarjeta conectada al motor",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Tarja connectada al motor",
    "����� ����������� � ��������",
    "",// TODO
    "",// TODO
    "",// TODO
  },
  { "Are you sure?",
    "Sind sie sicher?",
    "",// TODO
    "Sei sicuro?",
    "",// TODO
    "",// TODO
    "Etes vous sur?",
    "",// TODO
    "",// TODO
    "",// TODO
    "�Est� seguro?",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Est� segur?",
    "�� �������?",
    "",// TODO
    "",// TODO
    "",// TODO
  },
  { "Actuator",
    "",// TODO
    "",// TODO
    "Attuatore",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Actuador",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Actuador",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
  },
  { "Linear or h-h actuator control",
    "",// TODO
    "",// TODO
    "Controlla un attuatore lineare o h-h",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Controla un actuador lineal o h-h",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Controla un actuador lineal o h-h",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
  },
  { NULL }
  };

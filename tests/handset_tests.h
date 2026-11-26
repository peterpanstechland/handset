#include <QObject>

class HandsetTests : public QObject {
    Q_OBJECT

private slots:
    void dialNumberAcceptsAPhoneNumber();
    void dialNumberRejectsShellAndMarkup();
    void plainStripsMarkup();
    void incomingRingWinsOverAnotherLiveCall();
    void contactsRoundTripAndRejectBadInput();
    void placingACallCreatesThenStarts();
    void aFailedStartDeletesTheUnstartedCall();
    void anIncomingCallIsListedAndThenClosed();
    void themeFallsBack();
    void themeReadsAccent();
    void themeFollowsHyprlandRounding();
    void translatorFallsBackToEnglish();
    void translatorFollowsTheLanguageFile();
    void translatorPicksPluralForms();
};

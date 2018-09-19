//
// MainPage.xaml.cpp
// Implementation of the MainPage class.
//

#include "pch.h"
#include "MainPage.xaml.h"

using namespace BasicExample;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;



// The Blank Page item template is documented at https://go.microsoft.com/fwlink/?LinkId=402352&clcid=0x409

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	((std::string*)userp)->append((char*)contents, size * nmemb);
	return size * nmemb;
}

MainPage::MainPage()
{
	InitializeComponent();

	email = L"";
	password = L"";
	profileId = L"";
	anonId = L"";

	//_bc->initialize(serverUrl, secret, appId, version, company, appName);
	
}



void BasicExample::MainPage::Button_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	auto button = (Button^)sender;
	auto tag = button->Tag->ToString();


	if (tag == "Login") {

	}

	if (tag == "Reconnect") {

	}

	if (tag == "ForgotPassword") {

	}
}


void BasicExample::MainPage::TextBox_TextChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::TextChangedEventArgs^ e)
{
	auto textbox = (TextBox^)sender;
	auto tag = textbox->Tag->ToString();

	if (tag == "Email") {
		email = textbox->Text;
	}

	if (tag == "Password") {
		password = textbox->Text;
	}

	if (tag == "ProfileId") {
		profileId = textbox->Text;
	}

	if (tag == "AnonId") {
		anonId = textbox->Text;
	}
}

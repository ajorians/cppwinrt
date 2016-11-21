#include "pch.h"

#include <string>

using namespace std;

using namespace winrt;

using namespace Windows::ApplicationModel::Activation;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation;
using namespace Windows::UI;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Documents;
using namespace Windows::Media::SpeechRecognition;
using namespace Windows::Media::Capture;
using namespace Windows::UI::Core;

struct App : ApplicationT<App>
{
protected:
	hstring m_str;

	TextBox m_textbox;

	Button m_btnDictate;

	Button m_btnClearText;

	SpeechRecognizer m_Recognizer = nullptr;

public:
	void OnLaunched(LaunchActivatedEventArgs const &)
	{
		UIElement uiControls = SetupUIControls();

		m_btnDictate.Click([this](IInspectable const&, RoutedEventArgs const&) -> fire_and_forget
		{
			co_await InitializeRecognizer(m_textbox, m_btnDictate);

			if (m_Recognizer.State() == SpeechRecognizerState::Idle ||
				m_Recognizer.State() == SpeechRecognizerState::Paused)
			{
				co_await m_Recognizer.ContinuousRecognitionSession().StartAsync();
			}
			else
			{
				co_await m_Recognizer.ContinuousRecognitionSession().StopAsync();
			}

			co_return;
		});

		m_btnClearText.Click([this](IInspectable const&, RoutedEventArgs const&)
		{
			m_str = L"";
			m_textbox.Text(m_str);
		});

		Window window = Window::Current();
		window.Content(uiControls);
		window.Activate();

		DoMicrophonePermissionsAsync(m_btnDictate);
	}

protected:
	fire_and_forget DoMicrophonePermissionsAsync(Button dictateButton)
	{
		bool permissions = co_await RequestMicrophonePermissions();

		dictateButton.IsEnabled(permissions);
	}
	IAsyncOperation<bool> RequestMicrophonePermissions()
	{
		try
		{
			MediaCaptureInitializationSettings settings;
			settings.StreamingCaptureMode(StreamingCaptureMode::Audio);
			settings.MediaCategory(MediaCategory::Speech);
			MediaCapture capture;

			co_await capture.InitializeAsync(settings);
		}
		catch (hresult_error const &)
		{
			co_return false;
		}

		co_return true;
	}

	UIElement SetupUIControls()
	{
		Grid g;
		g.HorizontalAlignment(HorizontalAlignment::Stretch);
		g.VerticalAlignment(VerticalAlignment::Stretch);

		RowDefinition linkRow;
		linkRow.Height(GridLength{ 50.0, GridUnitType::Pixel });
		RowDefinition buttonsRow;
		buttonsRow.Height(GridLength{ 100.0, GridUnitType::Pixel });
		RowDefinition textBoxRow;
		textBoxRow.Height(GridLength{ 1.0, GridUnitType::Star });//Use remaining space

		auto rowCollections = g.RowDefinitions();

		rowCollections.Append(linkRow);
		rowCollections.Append(buttonsRow);
		rowCollections.Append(textBoxRow);

		//Setup the permissions not accepted textblock
		TextBlock permissionsText;
		g.Children().Append(permissionsText);
		Grid::SetRow(permissionsText, 0);
		permissionsText.TextWrapping(TextWrapping::WrapWholeWords);

		Run run1;
		run1.Text(L"The speech recognition privacy settings have not been accepted. ");
		Run run2;
		run2.Text(L"Open Privacy Settings");
		Run run3;
		run3.Text(L" to review the privacy policy and enable personalization.");

		Hyperlink link;
		link.Inlines().Append(run2);
		link.Click([](Hyperlink const&, HyperlinkClickEventArgs const&) -> fire_and_forget
		{
			Windows::System::Launcher::LaunchUriAsync(Uri(L"ms-settings:privacy-speechtyping"));
			co_return;
		});

		permissionsText.Inlines().Append(run1);
		permissionsText.Inlines().Append(link);
		permissionsText.Inlines().Append(run3);

		//Setup the butons in a horizontal stack panel
		StackPanel panelHorizontal;
		panelHorizontal.Orientation(Orientation::Horizontal);
		panelHorizontal.HorizontalAlignment(HorizontalAlignment::Center);

		g.Children().Append(panelHorizontal);
		Grid::SetRow(panelHorizontal, 1);

		Thickness buttonMargin{ 5.0/*L*/, 0.0/*T*/, 5.0/*R*/, 0.0/*B*/ };

		m_btnDictate.Content(PropertyValue::CreateString(L"Dictate"));
		panelHorizontal.Children().Append(m_btnDictate);
		m_btnDictate.Margin(buttonMargin);

		m_btnClearText.Content(PropertyValue::CreateString(L"Clear Text"));
		panelHorizontal.Children().Append(m_btnClearText);
		m_btnClearText.Margin(buttonMargin);

		//Setup the textbox
		g.Children().Append(m_textbox);
		Grid::SetRow(m_textbox, 2);
		m_textbox.IsReadOnly(true);
		m_textbox.TextWrapping(TextWrapping::Wrap);
		ScrollViewer::SetVerticalScrollBarVisibility(m_textbox, ScrollBarVisibility::Auto);

		return g;
	}

	IAsyncOperation<bool> InitializeRecognizer(TextBox outputTextbox, Button btnDictate)
	{
		if (m_Recognizer != nullptr)
		{
			//Already setup
			co_return true;
		}

		CoreDispatcher dispatcher = outputTextbox.Dispatcher();
		WINRT_ASSERT(dispatcher.HasThreadAccess());

		m_Recognizer = SpeechRecognizer(SpeechRecognizer::SystemSpeechLanguage());

		m_Recognizer.StateChanged([=](SpeechRecognizer const& sender, SpeechRecognizerStateChangedEventArgs const& args) -> fire_and_forget
		{
			WINRT_TRACE("State changed\n");

			co_await dispatcher.RunAsync(CoreDispatcherPriority::Normal, [=]()
			{
				if (args.State() == SpeechRecognizerState::Idle ||
					args.State() == SpeechRecognizerState::Paused)
				{
					btnDictate.Content(PropertyValue::CreateString(L"Dictate"));
				}
				else
				{
					btnDictate.Content(PropertyValue::CreateString(L"Stop Dictation"));
				}
			});
		});

		SpeechRecognitionTopicConstraint dictationContraint(SpeechRecognitionScenario::Dictation, L"dictation");
		m_Recognizer.Constraints().Append(dictationContraint);

		auto result = co_await m_Recognizer.CompileConstraintsAsync();

		if (result.Status() != SpeechRecognitionResultStatus::Success)
		{
			co_return false;
		}

		m_Recognizer.ContinuousRecognitionSession().Completed([=](SpeechContinuousRecognitionSession const&, SpeechContinuousRecognitionCompletedEventArgs const&)
		{
			WINRT_TRACE("Timed out\n");
		});

		m_Recognizer.ContinuousRecognitionSession().ResultGenerated([=](SpeechContinuousRecognitionSession const& sender, SpeechContinuousRecognitionResultGeneratedEventArgs const& args) -> fire_and_forget
		{
			auto confidence = args.Result().Confidence();

			if (confidence == SpeechRecognitionConfidence::High ||
				confidence == SpeechRecognitionConfidence::Medium)
			{
				std::wstring s = m_str;
				s += args.Result().Text();
				s += L" ";
				m_str = s;

				co_await dispatcher.RunAsync(CoreDispatcherPriority::Normal, [=]()
				{
					outputTextbox.Text(m_str);
				});
			}
		});

		m_Recognizer.HypothesisGenerated([=](SpeechRecognizer const& sender, SpeechRecognitionHypothesisGeneratedEventArgs const& args) -> fire_and_forget
		{
			auto hypothesis = args.Hypothesis().Text();

			std::wstring str = m_str;
			str += hypothesis;
			str += L"...";

			co_await dispatcher.RunAsync(CoreDispatcherPriority::Normal, [=]()
			{
				outputTextbox.Text(str);
			});

			co_return;
		});

		co_return true;
	}
};

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
	Application::Start([](auto &&) { make<App>(); });
}

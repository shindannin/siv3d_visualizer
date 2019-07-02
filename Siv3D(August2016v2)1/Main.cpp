# include <Siv3D.hpp>

#  pragma warning(disable:4996)


#include <vector>
#include <string>

using namespace std;
#define SZ(a) ((int)a.size()) 

// siv3dではなくwindows依存のコード。
// TODO : この部分は、Siv3Dライブラリに、機能追加されたはずなので、そちらを使用するようにする。
#include <windows.h> 
#include <tchar.h>
#include <stdio.h> 
#include <strsafe.h>
namespace s3d
{
	// class Server : 標準入出力
	class Server
	{
	private:

		HANDLE m_wo, m_ro, m_wi, m_ri;

		PROCESS_INFORMATION m_pi;

		bool m_connected = false;

	public:

		Server() = default;

		Server(FilePath path, bool show = true)
		{
			SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES) };
			sa.bInheritHandle = true;
			::CreatePipe(&m_ro, &m_wo, &sa, 0);
			::SetHandleInformation(m_ro, HANDLE_FLAG_INHERIT, 0);
			::CreatePipe(&m_ri, &m_wi, &sa, 0);
			::SetHandleInformation(m_wi, HANDLE_FLAG_INHERIT, 0);

			STARTUPINFO si = { sizeof(STARTUPINFO) };
			si.dwFlags = STARTF_USESTDHANDLES;
			si.hStdInput = m_ri;
			si.hStdOutput = m_wo;

			if (!show)
			{
				si.dwFlags |= STARTF_USESHOWWINDOW;
				si.wShowWindow = SW_HIDE;
			}

			FilePath currentPath = path.substr(0, path.lastIndexOf('/') + 1);
			m_connected = !!::CreateProcessW(path.c_str(), L"", nullptr, nullptr, TRUE, CREATE_NEW_CONSOLE, nullptr, currentPath.c_str(), &si, &m_pi);
		}

		~Server()
		{
			::WaitForSingleObject(m_pi.hProcess, INFINITE);
			::CloseHandle(m_pi.hThread);
			::CloseHandle(m_pi.hProcess);
			::CloseHandle(m_wi);
			::CloseHandle(m_ri);
			::CloseHandle(m_wo);
			::CloseHandle(m_ro);
		}

		explicit operator bool() const
		{
			return isConnected();
		}

		bool isConnected() const
		{
			return m_connected;
		}

		bool write(const std::string& cmd)
		{
			if (!m_connected)
			{
				return false;
			}

			DWORD written;
			return !!::WriteFile(m_wi, cmd.c_str(), static_cast<DWORD>(cmd.length()), &written, nullptr);
		}

		size_t available() const
		{
			if (!m_connected)
			{
				return 0;
			}

			DWORD n = 0;

			if (!::PeekNamedPipe(m_ro, 0, 0, 0, &n, 0))
			{
				return 0;
			}

			return n;
		}

		bool read(std::string& dst)
		{
			dst.clear();

			const size_t n = available();

			if (!n)
			{
				return false;
			}

			dst.resize(n);

			DWORD size;

			::ReadFile(m_ro, &dst[0], static_cast<DWORD>(n), &size, 0);

			return true;
		}
	};
}

void Split1(const string& str, vector<string>& out, const char splitter)
{
	out.clear();
	string::size_type st = 0;
	string::size_type next = 0;
	string tmp = str;
	do
	{
		next = tmp.find(splitter, st);
		string word = tmp.substr(st, next - st);
		if (word.length() >= 1) // 空文字列ありのときは消す
		{
			out.push_back(word);
		}
		st = next + 1;
	} while (next != string::npos);
}

void Trim(string& s)
{
	if (!s.empty() && s[s.size() - 1] == '\r')
	{
		s.erase(s.size() - 1);
	}
}

enum ShapeType
{
	ST_Line,
	ST_Rect,
	ST_Circle,
	ST_Font,
};



struct DrawShape
{
	bool operator<(const DrawShape& another) const
	{
		return time < another.time;
	}

	ShapeType type = ST_Line;
	float x = 0;
	float y = 0;
	float w = 0;
	float h = 0;
	Color color = Palette::White;
	string text;
	int time = 0;
	bool immediate = false;	//
};

bool CommandToShape(DrawShape& shape, const string& s)
{
	vector <string> args;
	Split1(s, args, ' ');

	if (SZ(args)==0)
	{
		return false;
	}

	if (args[0] == "Line")
	{
		shape.type = ST_Line;
	}
	else if (args[0] == "Rect")
	{
		shape.type = ST_Rect;
	}
	else if (args[0] == "Circle")
	{
		shape.type = ST_Circle;
	}
	else if (args[0] == "Font")
	{
		shape.type = ST_Font;
	}

	for (const auto& a : args)
	{
		vector <string> lr;
		Split1(a, lr, '=');
		if (SZ(lr) == 2)
		{
			const string& l = lr[0];
			Trim(lr[1]);
			const string& r = lr[1];

			if (l == "x")
			{
				shape.x = stof(r);
			}
			else if (l == "y")
			{
				shape.y = stof(r);
			}
			else if (l == "w")
			{
				shape.w = stof(r);
			}
			else if (l == "h")
			{
				shape.h = stof(r);
			}
			else if (l == "s")
			{
				shape.w = 
				shape.h = stof(r);
			}
			else if (l == "text")
			{
				shape.text = r;
			}
			else if (l == "color" || l == "c")
			{
				const unsigned long c = stoul(r, nullptr, 16);

				shape.color.r = (c >> 24) & 0xff;
				shape.color.g = (c >> 16) & 0xff;
				shape.color.b = (c >>  8) & 0xff;
				shape.color.a = c & 0xff;
			}
			else if (l == "time" || l == "t")
			{
				shape.time = stoi(r);
			}
			else if (l == "immediate" || l == "imm")
			{
				shape.immediate = true;
			}
		}
	}

	return true;
}

class SubProcess
{
public:
	void Open()
	{
		const auto path = Dialog::GetOpen({ { L"実行ファイル (*.exe)", L"*.exe" } });

		if (path)
		{
			mServer = new Server(path.value(), true); // false);
			mWaiting = true;
			mRawLog = "";
		}
	}

	void Close()
	{
		if (mServer != nullptr)
		{
			delete mServer;
			mServer = nullptr;
		}
	}

	bool Read()
	{
		string readStr;
		if (mServer->read(readStr))
		{
			mRawLog += readStr;
			const string done = "done\r\n";
			const string endout = mRawLog.substr(SZ(mRawLog) - SZ(done));

			if (endout == done)
			{
				vector <string> vs;
				Split1(mRawLog, vs, '\n');

				mShapes.clear();
				for (const string& s : vs)
				{
					mReadLogs.push_back(s);

					DrawShape shape;

					if (CommandToShape(shape, s))
					{
						mShapes.push_back(shape);
					}
				}
				sort(mShapes.begin(), mShapes.end());

				mServer->write("received\n");
				mWaiting = false;
				return true;
			}
		}

		return false;
	}

	bool IsWaiting() const
	{
		return mWaiting;
	}

	const vector <DrawShape>& GetShapes() const
	{
		return mShapes;
	}

private:
	Server * mServer = nullptr;					// 標準入出力

	string			mRawLog;
	vector <string>	mReadLogs;
	vector <DrawShape> mShapes;
	bool			mWaiting = false;
};


void Main()
{
	const static int WINDOW_W = 1280;
	const static int WINDOW_H =  960;
	Window::Resize(WINDOW_W, WINDOW_H);

	SubProcess sp;

	const Font font(10);
	GUI gui(GUIStyle::Default);
	gui.add(L"run", GUIButton::Create(L"実行"));
	const float slider_max = 640;
	gui.add(L"time", GUISlider::Create(0.0, slider_max, slider_max, 200));
	gui.setCenter(Point(WINDOW_W-160, 25));

	while (System::Update())
	{
		//========== 処理 ==========
		if (gui.button(L"run").pushed)
		{
			sp.Open();
		}

		if (sp.IsWaiting())
		{
			sp.Read();
			font(L"Waiting...").draw(1100, 100);
		}
		else
		{
			font(L"Done!").draw(1100, 100);
		}

//		font(gui.slider(L"time").value).draw(1000, 100);

		const vector <DrawShape>& shapes = sp.GetShapes();
		if (SZ(shapes) >= 1)
		{
			const int maxDrawTimeTime = shapes.back().time * gui.slider(L"time").value / slider_max;
			for (const DrawShape& sh : shapes)
			{
				if (sh.time <= maxDrawTimeTime)
				{
					switch (sh.type)
					{
					case ST_Line:
						Line(sh.x, sh.y, sh.x + sh.w, sh.y + sh.h).draw(5, sh.color);
						break;

					case ST_Rect:
						Rect(sh.x, sh.y, sh.w, sh.h).draw(sh.color);
						break;

					case ST_Circle:
						Circle(sh.x, sh.y, sh.w).draw(sh.color);
						break;

					case ST_Font:
						font(Widen(sh.text)).draw(sh.x, sh.y, sh.color);
						break;


					default:
						break;
					}
				}

			}

		const int maxDrawTime = shapes.back().time * gui.slider(L"time").value / slider_max;
		for (const DrawShape& sh : shapes)
		{
			if (sh.time <= maxDrawTime)
			{
				switch (sh.type)
				{
				case ST_Line:
					Line(sh.x, sh.y, sh.x + sh.w, sh.y + sh.h).draw(5, sh.color);
					break;

				case ST_Rect:
					Rect(sh.x, sh.y, sh.w, sh.h).draw(sh.color);
					break;

				case ST_Circle:
					Circle(sh.x, sh.y, sh.w).draw(sh.color);
					break;

				case ST_Font:
					font(Widen(sh.text)).draw(sh.x, sh.y, sh.color);
					break;


				default:
					break;
				}
			}

		}

		}


		//========== 終了 ==========
		if (Input::KeySpace.clicked)
		{
			sp.Close();
			break;
		}
	}
}


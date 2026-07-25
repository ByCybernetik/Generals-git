#pragma once

class EditorApp
{
public:
	bool init(const char *gameDataDir);
	void shutdown();

	bool isReady() const { return m_ready; }

private:
	bool m_ready = false;
};

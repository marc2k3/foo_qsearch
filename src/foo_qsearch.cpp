#define _WIN32_WINNT _WIN32_WINNT_WIN7
#define WINVER _WIN32_WINNT_WIN7

#include <array>
#include <SDK/foobar2000.h>

static constexpr const char* component_name = "QSearch";

static constexpr GUID guid_context_group = { 0x74ea6352, 0x3a06, 0x4ae7, { 0x92, 0x88, 0x1c, 0xaa, 0x6e, 0xf5, 0xd6, 0xc7 } };
static constexpr GUID guid_context_artist_is = { 0xcf64efae, 0x7c27, 0x4f44, { 0x88, 0xcf, 0xf6, 0xd, 0x1a, 0x2a, 0x90, 0x17 } };
static constexpr GUID guid_context_title_is = { 0xc3043a77, 0x114a, 0x41ec, { 0x99, 0x64, 0x34, 0x50, 0x66, 0x9f, 0xc8, 0x77 } };
static constexpr GUID guid_context_album_is = { 0x4bd0ae21, 0x3e48, 0x4ec4, { 0x90, 0xc, 0x3c, 0x14, 0x25, 0xc6, 0x28, 0x1e } };
static constexpr GUID guid_context_artist_has = { 0x5920ab47, 0x6b88, 0x4f0e, { 0x83, 0xde, 0x32, 0x6c, 0x1a, 0x56, 0x84, 0x83 } };
static constexpr GUID guid_context_title_has = { 0xf9febc3, 0x381, 0x4e2f, { 0xa3, 0xe5, 0x22, 0xbc, 0xd2, 0x7d, 0xee, 0x7e } };
static constexpr GUID guid_context_album_has = { 0x66956efb, 0xd24e, 0x4306, { 0xbc, 0x58, 0x20, 0xc5, 0x15, 0xe0, 0xae, 0x8a } };

static constexpr GUID guid_advconfig_branch = { 0x57cd1f9d, 0xb4fc, 0x46a9, { 0xad, 0x34, 0x20, 0x72, 0x39, 0x9c, 0x37, 0xa6 } };
static constexpr GUID guid_advconfig_autoplaylist = { 0x64b8d088, 0xcfa6, 0x472e, { 0x89, 0x35, 0x2a, 0x8c, 0x9d, 0x56, 0xdd, 0xe2 } };
static constexpr GUID guid_advconfig_autoplaylist_switch = { 0x9944325d, 0xd4e8, 0x4267, { 0x90, 0xf3, 0xfd, 0x66, 0x1b, 0xad, 0x2c, 0xd1 } };
static constexpr GUID guid_advconfig_search_window = { 0x525edd00, 0xc292, 0x4704, { 0xb6, 0xb8, 0xe, 0xca, 0x9c, 0xe8, 0x17, 0xb4 } };

static advconfig_branch_factory g_advconfig_branch(component_name, guid_advconfig_branch, advconfig_branch::guid_branch_tools, 0);
static advconfig_radio_factory g_advconfig_autoplaylist("Create Autoplaylist", guid_advconfig_autoplaylist, guid_advconfig_branch, 0, false);
static advconfig_radio_factory g_advconfig_autoplaylist_switch("Create Autoplaylist and switch", guid_advconfig_autoplaylist_switch, guid_advconfig_branch, 1, true);
static advconfig_radio_factory g_advconfig_search_window("Open Media Library search window", guid_advconfig_search_window, guid_advconfig_branch, 2, false);

DECLARE_COMPONENT_VERSION(
	component_name,
	"1.0.3",
	"Copyright (C) 2022 marc2003\n\n"
	"Build: " __TIME__ ", " __DATE__
);

VALIDATE_COMPONENT_FILENAME("foo_qsearch.dll");

static constexpr std::array context_guids =
{
	&guid_context_artist_is,
	&guid_context_title_is,
	&guid_context_album_is,
	&guid_context_artist_has,
	&guid_context_title_has,
	&guid_context_album_has,
};

class ContextMenu : public contextmenu_item_simple
{
public:
	GUID get_item_guid(uint32_t index) final
	{
		if (index >= context_guids.size()) FB2K_BugCheck();

		return *context_guids[index];
	}

	GUID get_parent() final
	{
		return guid_context_group;
	}

	bool context_get_display(uint32_t index, metadb_handle_list_cref handles, pfc::string_base& out, uint32_t&, const GUID&) final
	{
		if (index >= context_guids.size()) FB2K_BugCheck();

		get_item_name(index, out);
		return handles.get_count() == 1;
	}

	bool get_item_description(uint32_t index, pfc::string_base& out) final
	{
		if (index >= context_guids.size()) FB2K_BugCheck();

		get_item_name(index, out);
		return true;
	}

	uint32_t get_num_items() final
	{
		return static_cast<uint32_t>(context_guids.size());
	}

	void context_command(uint32_t index, metadb_handle_list_cref handles, const GUID&) final
	{
		if (index >= context_guids.size()) FB2K_BugCheck();
		if (handles.get_count() != 1) return;

		const pfc::string8 field = get_field(index);
		const pfc::string8 what = get_tf(field, handles[0]);
		if (what.is_empty())
		{
			FB2K_console_print(component_name, ": Not searching. ", field, " was not set or empty.");
			return;
		}

		const pfc::string8 query = pfc::format(field, " ", join(index), " ", what.toLower());

		if (g_advconfig_search_window.get() || IsKeyPressed(VK_SHIFT))
		{
			library_search_ui::get()->show(query);
		}
		else
		{
			create_autoplaylist(what, query);
		}
	}

	void get_item_name(uint32_t index, pfc::string_base& out) final
	{
		if (index >= context_guids.size()) FB2K_BugCheck();

		out = pfc::format(get_field(index), " ", join(index));
	}

private:
	bool check_query(const char* query)
	{
		try
		{
			return search_filter_manager_v2::get()->create_ex(query, fb2k::service_new<completion_notify_dummy>(), search_filter_manager_v2::KFlagSuppressNotify).is_valid();
		}
		catch (...) {}
		return false;
	}

	pfc::string8 get_field(uint32_t index)
	{
		if (index == 0 || index == 3) return "artist";
		if (index == 1 || index == 4) return "title";
		if (index == 2 || index == 5) return "album";
		return "";
	}

	pfc::string8 get_tf(const char* field, const metadb_handle_ptr& handle)
	{
		const pfc::string8 pattern = pfc::format("[%", field, "%]");
		titleformat_object_ptr obj;
		titleformat_compiler::get()->compile_safe(obj, pattern);

		auto pc = playback_control::get();
		metadb_handle_ptr np;
		pfc::string8 ret;

		if (pc->get_now_playing(np) && np->get_location() == handle->get_location())
		{
			pc->playback_format_title(nullptr, ret, obj, nullptr, playback_control::display_level_all);
		}
		else
		{
			handle->format_title(nullptr, ret, obj, nullptr);
		}
		return ret;
	}

	pfc::string8 join(uint32_t index)
	{
		return index < 3 ? "IS" : "HAS";
	}

	void create_autoplaylist(const char* name, const char* query)
	{
		if (!check_query(query)) return;

		auto plm = playlist_manager::get();
		const size_t playlist = plm->create_playlist(name, strlen(name), SIZE_MAX);
		autoplaylist_manager::get()->add_client_simple(query, "", playlist, 0);

		if (g_advconfig_autoplaylist_switch.get())
		{
			plm->set_active_playlist(playlist);
		}
	}
};

static contextmenu_group_popup_factory g_context_group(guid_context_group, contextmenu_groups::root, component_name, 0);
FB2K_SERVICE_FACTORY(ContextMenu);

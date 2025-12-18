#include <gtkmm.h>
#include <giomm/file.h>
#include <gdkmm/texture.h>
#include <SFML/Audio/Music.hpp>
#include <SFML/Audio/Sound.hpp>
#include <SFML/System/Time.hpp>

#include <iostream>
#include <vector>
#include <string>
#include <sstream>

class MusicPlayer : public Gtk::Window {
public:
    MusicPlayer()
    {
        set_title("Music Player (SFML 3)");
        set_default_size(500, 400);

        set_child(main_box);
        main_box.set_orientation(Gtk::Orientation::VERTICAL);

        controls_box.set_orientation(Gtk::Orientation::HORIZONTAL);
        main_box.append(controls_box);

        auto css = Gtk::CssProvider::create();
        css->load_from_data(R"(
        .control-button {
            padding: 8px;
            border-radius: 12px;
        }

        .control-button:hover {
            background-color: rgba(255,255,255,0.08);
        }

        .control-button:active {
            background-color: rgba(255,255,255,0.15);
        }
        )");

        Gtk::StyleContext::add_provider_for_display(
            Gdk::Display::get_default(),
            css,
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );


        btn_open = create_icon_button("icons/open.svg");
        btn_prev = create_icon_button("icons/prev.svg");
        btn_play = create_icon_button("icons/play.svg");
        btn_stop = create_icon_button("icons/stop.svg");
        btn_next = create_icon_button("icons/next.svg");

        btn_play->add_css_class("control-button");
        btn_next->add_css_class("control-button");
        btn_prev->add_css_class("control-button");
        btn_stop->add_css_class("control-button");
        btn_open->add_css_class("control-button");


        main_box.set_orientation(Gtk::Orientation::VERTICAL);
        main_box.set_spacing(8);
        main_box.append(controls_box);


        controls_box.set_orientation(Gtk::Orientation::HORIZONTAL);
        controls_box.set_spacing(16);
        controls_box.set_halign(Gtk::Align::CENTER);
        controls_box.set_margin_top(12);
        controls_box.set_margin_bottom(12);
        controls_box.set_margin_start(12);
        controls_box.set_margin_end(12);


        controls_box.append(*btn_open);
        controls_box.append(*btn_prev);
        controls_box.append(*btn_play);
        controls_box.append(*btn_next);
        controls_box.append(*btn_stop);



        progress_box.set_orientation(Gtk::Orientation::HORIZONTAL);
        main_box.append(progress_box);

        // ===== VOLUME BOX =====
        auto volume_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
        volume_box->set_margin(6);

        volume_label.set_text("🔊 100%");
        volume_label.set_size_request(60, -1);

        volume_scale.set_range(0, 100);
        volume_scale.set_value(100);
        volume_scale.set_digits(0);          // ⬅ TANPA DESIMAL
        volume_scale.set_increments(1, 10);  // ⬅ STEP 1
        volume_scale.set_hexpand(true);
        volume_scale.set_draw_value(false);  // ⬅ angka kita tampilkan sendiri
        volume_scale.signal_value_changed().connect(sigc::mem_fun(*this, &MusicPlayer::on_volume_changed));


        volume_box->append(volume_label);
        volume_box->append(volume_scale);
        main_box.append(*volume_box);


        progress_scale.set_draw_value(false);
        progress_scale.set_hexpand(true);

        progress_scale.signal_change_value().connect(
            sigc::mem_fun(*this, &MusicPlayer::on_seek), false);


        progress_box.append(progress_scale);
        progress_box.append(time_label);

        time_label.set_text("00:00 / 00:00");

        main_box.append(playlist_box);
        playlist_box.set_vexpand(true);
        playlist_box.set_selection_mode(Gtk::SelectionMode::SINGLE);


        btn_open->signal_clicked().connect(sigc::mem_fun(*this, &MusicPlayer::on_open));
        btn_play->signal_clicked().connect(sigc::mem_fun(*this, &MusicPlayer::on_play_pause));
        btn_stop->signal_clicked().connect(sigc::mem_fun(*this, &MusicPlayer::on_stop));
        btn_prev->signal_clicked().connect(sigc::mem_fun(*this, &MusicPlayer::on_prev));
        btn_next->signal_clicked().connect(sigc::mem_fun(*this, &MusicPlayer::on_next));


        playlist_box.signal_row_activated()
            .connect(sigc::mem_fun(*this, &MusicPlayer::on_row_activated));

        Glib::signal_timeout()
            .connect(sigc::mem_fun(*this, &MusicPlayer::update_progress), 500);
    }

private:
    Gtk::Box main_box, controls_box, progress_box;
    Gtk::Button *btn_open;
    Gtk::Button *btn_play;
    Gtk::Button *btn_stop;
    Gtk::Button *btn_prev;
    Gtk::Button *btn_next;
    Gtk::Scale progress_scale{Gtk::Orientation::HORIZONTAL};
    Gtk::Label time_label;
    Gtk::ListBox playlist_box;
    Gtk::Scale volume_scale{Gtk::Orientation::HORIZONTAL};
    Gtk::Label volume_label;


    Gtk::Button* create_icon_button(const std::string& icon_path, int size = 24)
{
    auto button = Gtk::make_managed<Gtk::Button>();

    auto file = Gio::File::create_for_path(icon_path);
    auto texture = Gdk::Texture::create_from_file(file);
    auto image = Gtk::make_managed<Gtk::Image>(texture);

    image->set_pixel_size(size);
    button->set_child(*image);
    button->set_has_frame(false);

    return button;
}




    struct PlaylistItem {
    Gtk::ListBoxRow* row;
    Gtk::Label* icon;
    Gtk::Label* title;
};

    sf::Music music;
    std::vector<PlaylistItem> playlist_items;
    std::vector<std::string> playlist;
    int current_index = -1;
    bool slider_dragging = false;
    bool song_finished = false;


    std::string filename_only(const std::string& path)
{
    auto pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? path : path.substr(pos + 1);
}


    bool on_seek(Gtk::ScrollType /*type*/, double value)
    {
    slider_dragging = true;

    if (music.getStatus() != sf::Sound::Status::Stopped) {
        music.setPlayingOffset(sf::seconds(static_cast<float>(value)));
    }

    slider_dragging = false;
    return false; // jangan blok GTK
    }

    void select_playlist_row(int index)
{
    auto row = playlist_box.get_row_at_index(index);
    if (row) {
        playlist_box.select_row(*row);
    }
}

void set_play_icon(const std::string& icon_path)
{
    auto file = Gio::File::create_for_path(icon_path);
    auto texture = Gdk::Texture::create_from_file(file);
    auto image = Gtk::make_managed<Gtk::Image>(texture);

    image->set_pixel_size(32);
    btn_play->set_child(*image);
}




    void on_volume_changed()
{
    int vol = static_cast<int>(volume_scale.get_value());

    music.setVolume(static_cast<float>(vol));

    volume_label.set_text("🔊 " + std::to_string(vol) + "%");
}

    void on_open()
{
    auto dialog = new Gtk::FileChooserDialog(
        "Select Music File",
        Gtk::FileChooser::Action::OPEN
    );

    dialog->set_transient_for(*this);
    dialog->add_button("_Cancel", Gtk::ResponseType::CANCEL);
    dialog->add_button("_Open", Gtk::ResponseType::ACCEPT);

    dialog->signal_response().connect([this, dialog](int response) {
        if (response == Gtk::ResponseType::ACCEPT) {
            auto gfile = dialog->get_file();
            if (!gfile) return;

            std::string path = gfile->get_path();
            playlist.push_back(path);

            // ===== UI Row =====
            auto row  = Gtk::make_managed<Gtk::ListBoxRow>();
            auto box  = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
            auto icon = Gtk::make_managed<Gtk::Label>("");
            auto name = Gtk::make_managed<Gtk::Label>(filename_only(path));

            box->append(*icon);
            box->append(*name);
            row->set_child(*box);

            playlist_box.append(*row);

            // ❗ JANGAN pakai { } langsung
            PlaylistItem item;
            item.row   = row;
            item.icon  = icon;
            item.title = name;
            playlist_items.push_back(item);

            if (current_index < 0) {
                current_index = 0;
                load_and_play(playlist[0]);
            }
        }

        dialog->hide();
        delete dialog;
    });

    dialog->show();
}


    void update_playlist_icons()
{
    for (size_t i = 0; i < playlist_items.size(); ++i) {
        if ((int)i == current_index) {
            if (music.getStatus() == sf::SoundSource::Status::Playing)
                playlist_items[i].icon->set_text("▶");
            else if (music.getStatus() == sf::SoundSource::Status::Paused)
                playlist_items[i].icon->set_text("⏸");
            else
                playlist_items[i].icon->set_text("");
        } else {
            playlist_items[i].icon->set_text("");
        }
    }
}



    void on_play_pause()
{
    auto status = music.getStatus();

    if (status == sf::Sound::Status::Playing) {
        music.pause();
        set_play_icon("icons/play.svg");
    }
    else {
        if (status == sf::Sound::Status::Stopped && current_index >= 0)
            load_and_play(playlist[current_index]);
        else
            music.play();

        set_play_icon("icons/pause.svg");
    }

    update_playlist_icons();
}





    void on_play()
    {
        if (music.getStatus() == sf::Sound::Status::Paused)
            music.play();
        else if (current_index >= 0)
            load_and_play(playlist[current_index]);
    }


    void on_pause() { music.pause(); }

    void on_stop()
{
    music.stop();
    progress_scale.set_value(0);
    set_play_icon("icons/play.svg");
}



    void on_prev()
    {
        if (current_index > 0)
            load_and_play(playlist[--current_index]);
    }

    void on_next()
    {
        if (current_index + 1 < (int)playlist.size())
            load_and_play(playlist[++current_index]);
    }

    void on_row_activated(Gtk::ListBoxRow* row)
    {
        current_index = row->get_index();
        load_and_play(playlist[current_index]);
    }

    void load_and_play(const std::string& file)
{
    if (!music.openFromFile(file)) {
        std::cerr << "Failed to load: " << file << '\n';
        return;
    }

    music.play();
    update_playlist_icons();
    set_play_icon("icons/pause.svg");

    progress_scale.set_range(
        0.0,
        music.getDuration().asSeconds()
    );

    select_playlist_row(current_index);
    update_playlist_icons();


}


    bool update_progress()
{
    if (music.getStatus() == sf::SoundSource::Status::Stopped &&
    current_index + 1 < (int)playlist.size()) {
    current_index++;
    load_and_play(playlist[current_index]);
}

    if (!slider_dragging &&
        music.getStatus() == sf::Sound::Status::Playing)
    {
        progress_scale.set_value(
            music.getPlayingOffset().asSeconds()
        );
    }

    time_label.set_text(
        format_time(music.getPlayingOffset().asSeconds()) +
        " / " +
        format_time(music.getDuration().asSeconds())
    );

    // AUTO NEXT
    if (!song_finished &&
        music.getStatus() == sf::Sound::Status::Stopped &&
        current_index >= 0)
    {
        song_finished = true;

        if (current_index + 1 < (int)playlist.size()) {
            current_index++;
            load_and_play(playlist[current_index]);
        } else {
            progress_scale.set_value(0.0);
        }
    }
    update_playlist_icons();

    return true;
}


    std::string format_time(int sec)
    {
        std::ostringstream oss;
        oss << sec / 60 << ':' << (sec % 60 < 10 ? "0" : "") << sec % 60;
        return oss.str();
    }
};

int main(int argc, char* argv[])
{
    auto app = Gtk::Application::create("org.sfml3.musicplayer");
    return app->make_window_and_run<MusicPlayer>(argc, argv);
}

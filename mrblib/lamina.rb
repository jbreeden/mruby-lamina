#<
# # Module Lamina
#>
module Lamina
  class << self
    #<
    # ## `attr_accessor :cache_path`
    # - Get or set the `cache_path` option.
    # - This tells lamina where to store cache data from the browser,
    #   such as localstorage and the like.
    # - If the specified path does not exists, lamina will create the directory.
    # - The parent of the indicated path must exist already.
    # - Default value: `nil` (No cache data will be saved)
    #>
    attr_accessor :cache_path

    #<
    # ## `attr_accessor :remote_debugging_port`
    # - Get or set the `remote_debugging_port` option (must be an int).
    # - This tells lamina what port (if any) to open for remote debugging
    #   via chrome dev tools.
    # - If specified, after launching the application you can navigate to
    #   `http://localhost:#{remote_debugging_port}` in a chrome window to
    #   access chrom dev tools for you app, inspect the html, and run javascript.
    # - Default value: `0` (disable remote debugging)
    #>
    attr_accessor :remote_debugging_port

    #<
    # ## `attr_accessor :v8_extensions`
    # - Get or set the `v8_extensions` array (must be an array of file paths)
    # - This tells lamina what files to load when a new V8 context is created.
    # - In these file, you can use the [`mruby-cef`](https://github.com/jbreeden/mruby-cef/blob/master/doc/src/mruby_cef_v8.md)
    #   API to write javascript extension for use in your application. (See `javascript_interop` sample for examples).
    # - If any file does not exist, a message will be logged to stdout (unless logging is disabled).
    # - Default value: Array with all file names ending in '.rb' from `/opt/lamina/js_extensions/` directory
    #>
    attr_accessor :js_extensions

    #<
    # ## `attr_accessor :url`
    # - Get or set the `url` option (must be a string)
    # - This tells lamina what url to load on app launch
    # - Accepts `file://`, `http://`, and `https://` urls
    # - Default value: `nil` (This is required however, you _must_ overwrite the default)
    #>
    attr_accessor :url

    #<
    # ## `attr_accessor :use_page_titles`
    # - Get or set the `use_page_titles` option (boolean)
    # - This tells lamina whether to update the title bar with the page title specified in
    #   any loaded page.
    # - Default value: `false` (The `window_title` option will be used for the liftime of the app)
    #>
    attr_accessor :use_page_titles

    #<
    # ## `attr_accessor :window_title`
    # - Get or set the `window_title` option (must be a string)
    # - This tells lamina what text to display in the application's title bar
    # - Default value: `Lamina`
    #>
    attr_accessor :window_title
  end

  def self.main
    if File.exists?("./lamina_main.rb")
      $stdout.puts "Loading lamina_main.rb"
      load "./lamina_main.rb"
    elsif File.exists?("./index.html")
      $stdout.puts "Loading index.html"
      self.url = "file://#{Dir.pwd}/index.html"
      self.run
    else
      $stdout.puts "No lamina_main.rb or index.html found. Opening directory."
      self.url = "file://#{Dir.pwd}"
      self.run
    end
  end

  def self.run
    ensure_lock_file_exists
    launch_mode = determine_launch_mode
    case launch_mode
    when :launching
      launch
    when :cef_process
      read_lamina_options
      start_cef_proc
    when :relaunching
      read_lamina_options
      relaunch
    end
  rescue Exception => ex
    $stderr.puts "Exception: #{ex}"
    $stderr.puts "Backtrace: #{ex.backtrace}"
  end

  def self.ensure_lock_file_exists
    unless File.exists?(@lock_file_path)
      puts "Lamina.ensure_lock_file_exists: Creating lock file"
      # Don't set @lock_file variable until opened for reading (when we get the lock)
      File.open(@lock_file_path, 'w') do |lock_file|
        lock_file.puts "LAMINA LOCK FILE"
      end
    end
  end

  def self.determine_launch_mode
    puts "Lamina.determine_launch_mode: Determining launch mode"

    # - Not closing this file because the shared lock
    # should be maintained as long as this app is running.
    # - On some platforms, file must be open for writing to get an exclusive lock,
    #   and reading for a shared lock, so opening with 'a' mode (read & write)
    puts "Lamina.determine_launch_mode: Opening lock file: #{@lock_file_path}"
    @lock_file = File.open(@lock_file_path, 'a+')
    # Child processes may get redundant exclusive locks due to file descriptors being shared.
    # So, check if this is a subprocess first. (TODO: Double check this)
    if ENV['CEF_SUBPROC']
      puts "Lamina.determine_launch_mode: CEF_SUBPROC environment variable was found. This is a CEF sub process"
      :cef_process
    elsif @lock_file.flock(File::LOCK_EX | File::LOCK_NB)
      puts "Lamina.determine_launch_mode: Got exclusive lock on lock file. App is being launched by user"
      :launching
    else
      puts "Lamina.determine_launch_mode: App is being relaunched by user"
      :relaunching
    end
  end

  #<
  # ## `::on_launch(&block)`
  # - Provide a block to lamina specifying launch behavior.
  # - The provided block is only called when the app is started, and no existing
  #   instances are running.
  # - This is where you should specify the application options such as the url to load, etc.
  #   + Options are set by using the `attr_accessor`s described above.
  #   + Options set in the `on_launch` block are automatically loaded if another
  #     app instance is launched (see: `::on_relaunch`)
  #>
  def self.on_launch(&block)
    @on_launch_proc = block
  end

  def self.launch
    puts "Switching to shared lock"
    @lock_file.flock(File::LOCK_SH)

    if @on_launch_proc
      puts "Lamina.launch: Running on_launch callback"
      @on_launch_proc[]
    end
    puts "Lamina.launch: Writing options to #{@options_file_path}"
    File.open(@options_file_path, 'w') do |opt_file|
      print_options(opt_file)
    end
    puts "Lamina.launch: Validating options"
    validate_options
    puts "Lamina.launch: Launching options:"
    print_options($stdout, '  ')

    puts "Lamina.launch: Starting CEF"
    start_cef_proc
  end

  #<
  # ## `::on_relaunch(&block)`
  # - Provide a block to lamina specifying relaunch behavior.
  # - The provided block is only called when the app is started, but an instance
  #   is already running.
  # - The lamina options (such as `url`, `window_title`, etc.) set for the running
  #   application instance are re-used automatically
  #>
  def self.on_relaunch(&block)
    @on_relaunch_block = block
  end

  #<
  # ## `::open_new_window`
  # - Should only be called in the `on_relaunch` block.
  # - Opens a new window for the application.
  # - Same as running `window.open('/');` in the existing browser window.
  #>
  # (This method is defined in mruby_lamina.cpp, but documented here to keep docs in one file)

  def self.relaunch
    File.open(@options_file_path, 'r') do |f|
      puts "Lamina.relaunch: Relaunching lamina with options:"
      print_options($stdout, '  ')
      @on_relaunch_block[] if @on_relaunch_block
    end
  end

  def self.read_lamina_options
    return unless File.exists?(@options_file_path)
    File.open(@options_file_path, 'r') do |f|
      puts "Lamina.read_lamina_options: Reading options from #{@options_file_path}"
      eval f.read
    end
  end

  def self.print_options(file, indent = '')
    file.puts "#{indent}Lamina.cache_path = #{ @cache_path ? "'#{@cache_path}'" : 'nil' }"
    file.puts "#{indent}Lamina.remote_debugging_port = #{@remote_debugging_port || 'nil' }"
    file.puts "#{indent}Lamina.js_extensions = #{ @js_extensions ? "#{@js_extensions}" : 'nil' }"
    file.puts "#{indent}Lamina.url = #{ @url ? "'#{@url}'" : 'nil' }"
    file.puts "#{indent}Lamina.use_page_titles = #{@use_page_titles || 'false' }"
    file.puts "#{indent}Lamina.window_title = #{ @window_title ? "'#{@window_title}'" : 'nil' }"
  end

  def self.validate_options
    unless @cache_path.nil? || Dir.exists?(@cache_path)
      begin
        Dir.mkdir @cache_path
      rescue Exception => ex
        puts "!!! ERROR !!! Lamina.cache_path (#{@cache_path}), directory does not exist and could not be created."
        puts ex
        exit 1
      end
    end

    @js_extensions.each do |f|
      unless File.exists? f
        puts "!!! ERROR !!! Lamina.js_extensions entry #{f} does not exist"
        exit 1
      end
    end

    unless @remote_debugging_port.nil? || @remote_debugging_port.kind_of?(Fixnum)
      puts "!!! ERROR !!! If Lamina.remoute_debugging_port is specified, it must be an int"
      exit 1
    end

    unless @url =~ %r[(https?|file)://.*]
      puts "!!! ERROR !!! Lamina.url must be a string matching %r[(https?|file)://.*]"
      exit 1
    end

    # @use_page_titles can be anything, if the user doesn't set nil
    # or false it will evaluate as true... no biggy, so no validation

    unless @window_title.nil? || @window_title.kind_of?(String)
      puts "!!! ERROR !!! If Lamina.window_title is specified, it must be a string"
      exit 1
    end
  end

  # Configure Defaults on load
  # --------------------------

  if Dir.exists?('./lamina_cache')
    @cache_path = './lamina_cache'
  else
    @cache_path = nil
  end
  if Dir.exists?('/opt/lamina/js_extensions')
    @js_extensions = Dir.entries('/opt/lamina/js_extensions').reject { |f|
      f =~ /^\.\.?$/ || !File.file?("/opt/lamina/js_extensions/#{f}")
    }.map { |f|
      "/opt/lamina/js_extensions/#{f}"
    }
  else
    @js_extensions = []
  end
  @remote_debugging_port = 9999
  @url = "file://#{Dir.pwd}/index.html"
  @use_page_titles = false
  @window_title = "Lamina"

  # Internal-only variables
  @lock_file_path = ".lamina"
  @options_file_path = ".lamina_options"
  @lock_file = nil
end

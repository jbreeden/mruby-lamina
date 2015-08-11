#<
# # Module Lamina
#>
module Lamina
  class << self
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
  end

  def self.main
    self.run
  end

  def self.run
    ensure_lock_file_exists
    launch_mode = determine_launch_mode
    case launch_mode
    when :launching
      launch
    when :cef_process
      start_cef_proc
    when :relaunching
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
    puts "Lamina.launch: Validating options"
    validate_options
    puts "Lamina.launch: Starting CEF"
    start_cef_proc
  end

  def self.validate_options
    @js_extensions.each do |f|
      unless File.exists? f
        puts "!!! ERROR !!! Lamina.js_extensions entry #{f} does not exist"
        exit 1
      end
    end
  end

  # Configure Defaults on load
  # --------------------------

  if Dir.exists?('/opt/lamina/js_extensions')
    @js_extensions = Dir.entries('/opt/lamina/js_extensions').reject { |f|
      f =~ /^\.\.?$/ || !File.file?("/opt/lamina/js_extensions/#{f}")
    }.map { |f|
      "/opt/lamina/js_extensions/#{f}"
    }
  else
    @js_extensions = []
  end

  # Internal-only variables
  @lock_file_path = ".lamina"
  @options_file_path = ".lamina_options"
  @lock_file = nil
end

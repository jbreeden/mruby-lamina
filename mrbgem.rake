def configure_mruby_lamina(conf)
  gem_dir = File.dirname(__FILE__)

  # Common include path (all platforms)
  conf.cc.include_paths << "#{gem_dir}/include"
  conf.cxx.include_paths << "#{gem_dir}/include"
end

MRuby::Gem::Specification.new('mruby-lamina') do |spec|
  spec.license = 'MIT'
  spec.author  = 'Jared Breeden'
  spec.summary = 'A Chromium shell for mruby apps'
  spec.bins = %w(lamina laminaw)
end

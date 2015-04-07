$mruby_lamina_gem_dir = File.expand_path(File.dirname(__FILE__))

def configure_mruby_lamina(conf)
  conf.cc.include_paths << "#{$mruby_lamina_gem_dir}/include"
  conf.cxx.include_paths << "#{$mruby_lamina_gem_dir}/include"
end

MRuby::Gem::Specification.new('mruby-lamina') do |spec|
  spec.license = 'MIT'
  spec.author  = 'Jared Breeden'
  spec.summary = 'A Chromium shell for mruby apps'
  
  if ENV['OS'] =~ /windows/i
    spec.bins = %w(lamina laminaw)
  else
    spec.bins = %w(lamina)
  end
  
  spec.cc.flags << [ '-std=c11' ]
  spec.cxx.flags << [ '-std=c++11' ]
end

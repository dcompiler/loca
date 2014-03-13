module Machine
  def self.mac_get_machine_env
    env = Hash.new
    raw = `system_profiler`
    lines = raw.split("\n")
    info = lines[4..16] << lines[74]
    info.each do |pair| 
      key, val = pair.split(":")
      env[key.gsub /^\s+/, ""] = val.gsub /^\s+/, "" 
    end
    return env
  end

  def self.get_machine_env
    env = Hash.new
    env[:uname] = %x[uname -a]
    return env
  end

  def self.get_output_file_name( cmd )
   require 'socket'
   require 'time'

    seed = cmd || $0 # command name
    path =
      "%s_%s_%s_%s_%d" % [
           Socket.gethostname,
           seed,
           Process.pid,
           Time.now.iso8601(2),
           rand(101010)
      ]
    # File.join( dir, filename )
    return path.gsub(%r/[^0-9a-zA-Z]/,'_').gsub(%r/\s+/, '_')
  end
end # module

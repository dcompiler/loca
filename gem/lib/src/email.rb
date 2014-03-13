require 'net/smtp'

def send_email(to,opts={})
  opts[:server]      ||= 'localhost'
  opts[:from]        ||= 'loca_user@unknown.site.com'
  opts[:from_alias]  ||= 'Loca User'
  opts[:subject]     ||= "Loca error report"
  opts[:body]        ||= "empty message"

  msg = <<END_OF_MESSAGE
From: #{opts[:from_alias]} <#{opts[:from]}>
To: <#{to}>
Subject: #{opts[:subject]}

#{opts[:body]}
END_OF_MESSAGE

begin
  Net::SMTP.start(opts[:server]) do |smtp|
    smtp.send_message msg, opts[:from], to
  end
rescue Exception
  warn "Loca cannot send the error report. Abort."
end

end
